
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2013, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"

#include "config/cfg_schema.h"

#include "core/olsr_logging.h"
#include "core/olsr_class.h"
#include "core/olsr_netaddr_acl.h"
#include "core/olsr_plugins.h"
#include "core/olsr_stream_socket.h"
#include "core/olsr_timer.h"
#include "core/olsr_subsystem.h"

#include "tools/olsr_cfg.h"
#include "tools/olsr_telnet.h"

/* static function prototypes */
static int _init(void);
static void _cleanup(void);

static void _call_stop_handler(struct olsr_telnet_data *data);
static void _cb_config_changed(void);
static int _cb_telnet_init(struct olsr_stream_session *);
static void _cb_telnet_cleanup(struct olsr_stream_session *);
static void _cb_telnet_create_error(struct olsr_stream_session *,
    enum olsr_stream_errors);
static enum olsr_stream_session_state _cb_telnet_receive_data(
    struct olsr_stream_session *);
static enum olsr_telnet_result _telnet_handle_command(
    struct olsr_telnet_data *);
static struct olsr_telnet_command *_check_telnet_command(
    struct olsr_telnet_data *data, const char *name,
    struct olsr_telnet_command *cmd);

static void _cb_telnet_repeat_timer(void *data);
static enum olsr_telnet_result _cb_telnet_quit(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_telnet_help(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_telnet_echo(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_telnet_repeat(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_telnet_timeout(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_telnet_version(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_telnet_plugin(struct olsr_telnet_data *data);

/* configuration of telnet server */
static struct cfg_schema_entry telnet_entries[] = {
  CFG_MAP_ACL_V46(olsr_stream_managed_config,
      acl, "acl", "127.0.0.1", "Access control list for telnet interface"),
  CFG_MAP_NETADDR_V4(olsr_stream_managed_config,
      bindto_v4, "bindto_v4", "127.0.0.1", "Bind telnet ipv4 socket to this address", false, true),
  CFG_MAP_NETADDR_V6(olsr_stream_managed_config,
      bindto_v6, "bindto_v6", "::1", "Bind telnet ipv6 socket to this address", false, true),
  CFG_MAP_INT_MINMAX(olsr_stream_managed_config,
      port, "port", "2006", "Network port for telnet interface", 1, 65535),
};

static struct cfg_schema_section telnet_section = {
  .type = "telnet",
  .mode = CFG_SSMODE_UNNAMED_OPTIONAL_STARTUP_TRIGGER,
  .help = "Settings for the telnet interface",
  .cb_delta_handler = _cb_config_changed,
  .entries = telnet_entries,
  .entry_count = ARRAYSIZE(telnet_entries),
};

/* built-in telnet commands */
static struct olsr_telnet_command _builtin[] = {
  TELNET_CMD("quit", _cb_telnet_quit, "Ends telnet session"),
  TELNET_CMD("exit", _cb_telnet_quit, "Ends telnet session"),
  TELNET_CMD("help", _cb_telnet_help,
      "help: Display the online help text and a list of commands"),
  TELNET_CMD("echo", _cb_telnet_echo,"echo <string>: Prints a string"),
  TELNET_CMD("repeat", _cb_telnet_repeat,
      "repeat <seconds> <command>: Repeats a telnet command every X seconds"),
  TELNET_CMD("timeout", _cb_telnet_timeout,
      "timeout <seconds> :Sets telnet session timeout"),
  TELNET_CMD("version", _cb_telnet_version, "Displays version of the program"),
  TELNET_CMD("plugin", _cb_telnet_plugin,
        "control plugins dynamically, parameters are 'list',"
        "'load <plugin>' and 'unload <plugin>'"),
};

/* subsystem definition */
struct oonf_subsystem oonf_telnet_subsystem = {
  .init = _init,
  .cleanup = _cleanup,
  .cfg_section = &telnet_section,
};

/* telnet session handling */
static struct olsr_class _telnet_memcookie = {
  .name = "telnet session",
  .size = sizeof(struct olsr_telnet_session),
};
static struct olsr_timer_info _telnet_repeat_timerinfo = {
  .name = "txt repeat timer",
  .callback = _cb_telnet_repeat_timer,
  .periodic = true,
};

static struct olsr_stream_managed _telnet_managed = {
  .config = {
    .session_timeout = 120000, /* 120 seconds */
    .maximum_input_buffer = 4096,
    .allowed_sessions = 3,
    .memcookie = &_telnet_memcookie,
    .init = _cb_telnet_init,
    .cleanup = _cb_telnet_cleanup,
    .receive_data = _cb_telnet_receive_data,
    .create_error = _cb_telnet_create_error,
  },
};

struct avl_tree telnet_cmd_tree;

/**
 * Initialize telnet subsystem
 * @return always returns 0
 */
static int
_init(void) {
  size_t i;

  olsr_class_add(&_telnet_memcookie);
  olsr_timer_add(&_telnet_repeat_timerinfo );

  olsr_stream_add_managed(&_telnet_managed);

  /* initialize telnet commands */
  avl_init(&telnet_cmd_tree, avl_comp_strcasecmp, false);
  for (i=0; i<ARRAYSIZE(_builtin); i++) {
    olsr_telnet_add(&_builtin[i]);
  }
  return 0;
}

/**
 * Cleanup all allocated data of telnet subsystem
 */
static void
_cleanup(void) {
  olsr_stream_remove_managed(&_telnet_managed, true);
  olsr_class_remove(&_telnet_memcookie);
}

/**
 * Add a new telnet command to telnet subsystem
 * @param command pointer to initialized telnet command object
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_telnet_add(struct olsr_telnet_command *command) {
  command->_node.key = command->command;
  if (avl_insert(&telnet_cmd_tree, &command->_node)) {
    return -1;
  }
  return 0;
}

/**
 * Remove a telnet command from telnet subsystem
 * @param command pointer to telnet command object
 */
void
olsr_telnet_remove(struct olsr_telnet_command *command) {
  avl_remove(&telnet_cmd_tree, &command->_node);
}

/**
 * Stop a currently running continuous telnet command
 * @param data pointer to telnet data
 */
void
olsr_telnet_stop(struct olsr_telnet_data *data) {
  _call_stop_handler(data);
  data->show_echo = true;
  abuf_puts(data->out, "> ");
  olsr_telnet_flush_session(data);
}

/**
 * Execute a telnet command.
 * @param cmd pointer to name of command
 * @param para pointer to parameter string
 * @param out buffer for output of command
 * @param remote pointer to address which triggers the execution
 * @return result of telnet command
 */
enum olsr_telnet_result
olsr_telnet_execute(const char *cmd, const char *para,
    struct autobuf *out, struct netaddr *remote) {
  struct olsr_telnet_data data;
  enum olsr_telnet_result result;

  memset(&data, 0, sizeof(data));
  data.command = cmd;
  data.parameter = para;
  data.out = out;
  data.remote = remote;

  result = _telnet_handle_command(&data);
  olsr_telnet_stop(&data);
  return result;
}

/**
 * Handler for configuration changes
 */
static void
_cb_config_changed(void) {
  struct olsr_stream_managed_config config;

  /* generate binary config */
  memset(&config, 0, sizeof(config));
  if (cfg_schema_tobin(&config, telnet_section.post,
      telnet_entries, ARRAYSIZE(telnet_entries))) {
    /* error in conversion */
    OLSR_WARN(LOG_TELNET, "Cannot map telnet config to binary data");
    goto apply_config_failed;
  }

  if (olsr_stream_apply_managed(&_telnet_managed, &config)) {
    /* error while updating sockets */
    goto apply_config_failed;
  }

  /* fall through */
apply_config_failed:
  olsr_acl_remove(&config.acl);
}

/**
 * Initialization of incoming telnet session
 * @param session pointer to TCP session
 * @return 0
 */
static int
_cb_telnet_init(struct olsr_stream_session *session) {
  struct olsr_telnet_session *telnet_session;

  /* get telnet session pointer */
  telnet_session = (struct olsr_telnet_session *)session;

  telnet_session->data.show_echo = true;
  telnet_session->data.stop_handler = NULL;
  telnet_session->data.timeout_value = 120000;
  telnet_session->data.out = &telnet_session->session.out;
  telnet_session->data.remote = &telnet_session->session.remote_address;

  list_init_head(&telnet_session->data.cleanup_list);

  return 0;
}

/**
 * Cleanup of telnet session
 * @param session pointer to TCP session
 */
static void
_cb_telnet_cleanup(struct olsr_stream_session *session) {
  struct olsr_telnet_session *telnet_session;
  struct olsr_telnet_cleanup *handler, *it;

  /* get telnet session pointer */
  telnet_session = (struct olsr_telnet_session *)session;

  /* stop continuous commands */
  olsr_telnet_stop(&telnet_session->data);

  /* call all cleanup handlers */
  list_for_each_element_safe(&telnet_session->data.cleanup_list, handler, node, it) {
    /* remove from list first */
    olsr_telnet_remove_cleanup(handler);

    /* after this command the handler pointer might not be valid anymore */
    handler->cleanup_handler(handler);
  }
}

/**
 * Create error string for telnet session
 * @param session pointer to TCP stream
 * @param error TCP error code to generate
 */
static void
_cb_telnet_create_error(struct olsr_stream_session *session,
    enum olsr_stream_errors error) {
  switch(error) {
    case STREAM_REQUEST_TOO_LARGE:
      abuf_puts(&session->out, "Input buffer overflow, ending connection\n");
      break;
    case STREAM_SERVICE_UNAVAILABLE:
      abuf_puts(&session->out, "Telnet service unavailable, too many sessions\n");
      break;
    case STREAM_REQUEST_FORBIDDEN:
    default:
      /* no message */
      break;
  }
}

/**
 * Stop a continuous telnet command
 * @param data pointer to telnet data
 */
static void
_call_stop_handler(struct olsr_telnet_data *data) {
  void (*stop_handler)(struct olsr_telnet_data *);

  if (data->stop_handler) {
    /*
     * make sure that stop_handler is not set anymore when
     * it is called.
     */
    stop_handler = data->stop_handler;
    data->stop_handler = NULL;

    /* call stop handler */
    stop_handler(data);
  }
}

/**
 * Handler for receiving data from telnet session
 * @param session pointer to TCP session
 * @return TCP session state
 */
static enum olsr_stream_session_state
_cb_telnet_receive_data(struct olsr_stream_session *session) {
  static const char defaultCommand[] = "/link/neigh/topology/hna/mid/routes";
  static char tmpbuf[128];

  struct olsr_telnet_session *telnet_session;
  enum olsr_telnet_result cmd_result;
  char *eol;
  int len;
  bool processedCommand = false, chainCommands = false;

  /* get telnet session pointer */
  telnet_session = (struct olsr_telnet_session *)session;

  /* loop over input */
  while (abuf_getlen(&session->in) > 0) {
    char *para = NULL, *cmd = NULL, *next = NULL;

    /* search for end of line */
    eol = memchr(abuf_getptr(&session->in), '\n', abuf_getlen(&session->in));

    if (eol == NULL) {
      break;
    }

    /* terminate line with a 0 */
    if (eol != abuf_getptr(&session->in) && eol[-1] == '\r') {
      eol[-1] = 0;
    }
    *eol++ = 0;

    /* handle line */
    OLSR_DEBUG(LOG_TELNET, "Interactive console: %s\n", abuf_getptr(&session->in));
    cmd = abuf_getptr(&session->in);
    processedCommand = true;

    /* apply default command */
    if (strcmp(cmd, "/") == 0) {
      strcpy(tmpbuf, defaultCommand);
      cmd = tmpbuf;
    }

    if (cmd[0] == '/') {
      cmd++;
      chainCommands = true;
    }
    while (cmd) {
      len = abuf_getlen(&session->out);

      /* handle difference between multicommand and singlecommand mode */
      if (chainCommands) {
        next = strchr(cmd, '/');
        if (next) {
          *next++ = 0;
        }
      }
      para = strchr(cmd, ' ');
      if (para != NULL) {
        *para++ = 0;
      }

      /* if we are doing continous output, stop it ! */
      _call_stop_handler(&telnet_session->data);

      if (strlen(cmd) != 0) {
        OLSR_DEBUG(LOG_TELNET, "Processing telnet command: '%s' '%s'",
            cmd, para);

        telnet_session->data.command = cmd;
        telnet_session->data.parameter = para;

        cmd_result = _telnet_handle_command(&telnet_session->data);
        if (abuf_has_failed(telnet_session->data.out)) {
          cmd_result = TELNET_RESULT_INTERNAL_ERROR;
        }

        switch (cmd_result) {
          case TELNET_RESULT_ACTIVE:
            break;
          case TELNET_RESULT_CONTINOUS:
            telnet_session->data.show_echo = false;
            break;
          case _TELNET_RESULT_UNKNOWN_COMMAND:
            abuf_setlen(&session->out, len);
            abuf_appendf(&session->out, "Error, unknown command '%s'\n", cmd);
            break;
          case TELNET_RESULT_QUIT:
            return STREAM_SESSION_SEND_AND_QUIT;
          case TELNET_RESULT_INTERNAL_ERROR:
          default:
            /* reset stream */
            abuf_setlen(&session->out, len);
            abuf_appendf(&session->out,
                "Error in autobuffer during command '%s'.\n", cmd);
            break;
        }
        /* put an empty line behind each command */
        if (telnet_session->data.show_echo) {
          abuf_puts(&session->out, "\n");
        }
      }
      cmd = next;
    }

    /* remove line from input buffer */
    abuf_pull(&session->in, eol - abuf_getptr(&session->in));

    if (abuf_getptr(&session->in)[0] == '/') {
      /* end of multiple command line */
      return STREAM_SESSION_SEND_AND_QUIT;
    }
  }

  /* reset timeout */
  olsr_stream_set_timeout(session, telnet_session->data.timeout_value);

  /* print prompt */
  if (processedCommand && session->state == STREAM_SESSION_ACTIVE
      && telnet_session->data.show_echo) {
    abuf_puts(&session->out, "> ");
  }

  return STREAM_SESSION_ACTIVE;
}

/**
 * Helper function to call telnet command handler
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_telnet_handle_command(struct olsr_telnet_data *data) {
  struct olsr_telnet_command *cmd;
#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_INFO
  struct netaddr_str buf;
#endif
  cmd = _check_telnet_command(data, data->command, NULL);
  if (cmd == NULL) {
    return _TELNET_RESULT_UNKNOWN_COMMAND;
  }

  OLSR_INFO(LOG_TELNET, "Executing command from %s: %s %s",
      netaddr_to_string(&buf, data->remote), data->command,
      data->parameter == NULL ? "" : data->parameter);
  return cmd->handler(data);
}

/**
 * Checks for existing (and allowed) telnet command.
 * Either name or cmd should be NULL, but not both.
 * @param data pointer to telnet data
 * @param name pointer to command name (might be NULL)
 * @param cmd pointer to telnet command object (might be NULL)
 * @return telnet command object or NULL if not found or forbidden
 */
static struct olsr_telnet_command *
_check_telnet_command(struct olsr_telnet_data *data,
    const char *name, struct olsr_telnet_command *cmd) {
#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_DEBUG
  struct netaddr_str buf;
#endif

  // TODO: split into two functions
  if (cmd == NULL) {
    cmd = avl_find_element(&telnet_cmd_tree, name, cmd, _node);
    if (cmd == NULL) {
      return cmd;
    }
  }
  if (cmd->acl == NULL) {
    return cmd;
  }

  if (!olsr_acl_check_accept(cmd->acl, data->remote)) {
    OLSR_DEBUG(LOG_TELNET, "Blocked telnet command '%s' to '%s' because of acl",
        cmd->command, netaddr_to_string(&buf, data->remote));
    return NULL;
  }
  return cmd;
}

/**
 * Telnet command 'quit'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_quit(struct olsr_telnet_data *data __attribute__((unused))) {
  return TELNET_RESULT_QUIT;
}

/**
 * Telnet command 'help'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_help(struct olsr_telnet_data *data) {
  struct olsr_telnet_command *cmd;

  if (data->parameter != NULL && data->parameter[0] != 0) {
    cmd = _check_telnet_command(data, data->parameter, NULL);
    if (cmd == NULL) {
      abuf_appendf(data->out, "No help text found for command: %s\n", data->parameter);
      return TELNET_RESULT_ACTIVE;
    }

    if (cmd->help_handler) {
      cmd->help_handler(data);
    }
    else {
      if (abuf_appendf(data->out, "%s", cmd->help) < 0) {
        return TELNET_RESULT_INTERNAL_ERROR;
      }
    }
    return TELNET_RESULT_ACTIVE;
  }

  if (abuf_puts(data->out, "Known commands:\n") < 0) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }

  avl_for_each_element(&telnet_cmd_tree, cmd, _node) {
    if (_check_telnet_command(data, NULL, cmd)) {
      if (abuf_appendf(data->out, "  %s\n", cmd->command) < 0) {
        return TELNET_RESULT_INTERNAL_ERROR;
      }
    }
  }

  if (abuf_puts(data->out, "Use 'help <command> to see a help text for one command\n") < 0) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }
  return TELNET_RESULT_ACTIVE;
}

/**
 * Telnet command 'echo'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_echo(struct olsr_telnet_data *data) {

  if (abuf_appendf(data->out, "%s\n",
      data->parameter == NULL ? "" : data->parameter) < 0) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }
  return TELNET_RESULT_ACTIVE;
}

/**
 * Telnet command 'timeout'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_timeout(struct olsr_telnet_data *data) {
  if (data->parameter == NULL) {
    data->timeout_value = 0;
  }
  else {
    data->timeout_value = (uint32_t)strtoul(data->parameter, NULL, 10) * 1000;
  }
  return TELNET_RESULT_ACTIVE;
}

/**
 * Stop handler for repeating telnet commands
 * @param data pointer to telnet data
 */
static void
_cb_telnet_repeat_stophandler(struct olsr_telnet_data *data) {
  olsr_timer_stop((struct olsr_timer_entry *)data->stop_data[0]);
  free(data->stop_data[0]);
  free(data->stop_data[1]);

  data->stop_handler = NULL;
  data->stop_data[0] = NULL;
  data->stop_data[1] = NULL;
  data->stop_data[2] = NULL;
}

/**
 * Timer event handler for repeating telnet commands
 * @param ptr pointer to custom data
 */
static void
_cb_telnet_repeat_timer(void *ptr) {
  struct olsr_telnet_data *telnet_data = ptr;
  struct olsr_telnet_session *session;

  /* set command/parameter with repeat settings */
  telnet_data->command = telnet_data->stop_data[1];
  telnet_data->parameter = telnet_data->stop_data[2];

  if (_telnet_handle_command(telnet_data) != TELNET_RESULT_ACTIVE) {
    _call_stop_handler(telnet_data);
  }

  /* reconstruct original session pointer */
  session = container_of(telnet_data, struct olsr_telnet_session, data);
  olsr_stream_flush(&session->session);
}

/**
 * Telnet command 'repeat'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_repeat(struct olsr_telnet_data *data) {
  struct olsr_timer_entry *timer;
  int interval = 0;
  char *ptr = NULL;

  if (data->stop_handler) {
    abuf_puts(data->out, "Error, you cannot stack continous output commands\n");
    return TELNET_RESULT_ACTIVE;
  }

  if (data->parameter == NULL || (ptr = strchr(data->parameter, ' ')) == NULL) {
    abuf_puts(data->out, "Missing parameters for repeat\n");
    return TELNET_RESULT_ACTIVE;
  }

  ptr++;

  interval = atoi(data->parameter);
  if (interval < 1) {
    abuf_puts(data->out, "Please specify an interval >= 1\n");
    return TELNET_RESULT_ACTIVE;
  }

  timer = calloc(1, sizeof(*timer));
  if (timer == NULL) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }

  timer->cb_context = data;
  timer->info = &_telnet_repeat_timerinfo;
  olsr_timer_start(timer, interval * 1000);

  data->stop_handler = _cb_telnet_repeat_stophandler;
  data->stop_data[0] = timer;
  data->stop_data[1] = strdup(ptr);
  data->stop_data[2] = NULL;

  /* split command/parameter and remember it */
  ptr = strchr(data->stop_data[1], ' ');
  if (ptr != NULL) {
    /* found a parameter */
    *ptr++ = 0;
    data->stop_data[2] = ptr;
  }

  /* start command the first time */
  data->command = data->stop_data[1];
  data->parameter = data->stop_data[2];

  if (_telnet_handle_command(data) != TELNET_RESULT_ACTIVE) {
    _call_stop_handler(data);
  }

  return TELNET_RESULT_CONTINOUS;
}

/**
 * Telnet command 'version'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_version(struct olsr_telnet_data *data) {
  olsr_log_printversion(data->out);
  return TELNET_RESULT_ACTIVE;
}

/**
 * Telnet command 'plugin'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_plugin(struct olsr_telnet_data *data) {
  struct oonf_subsystem *plugin;
  const char *plugin_name = NULL;

  if (data->parameter == NULL || strcasecmp(data->parameter, "list") == 0) {
    abuf_puts(data->out, "Plugins:\n");

    avl_for_each_element(&olsr_plugin_tree, plugin, _node) {
      abuf_appendf(data->out, "\t%s\n", plugin->name);
    }
    return TELNET_RESULT_ACTIVE;
  }

  plugin_name = strchr(data->parameter, ' ');
  if (plugin_name == NULL) {
    abuf_appendf(data->out, "Error, missing or unknown parameter\n");
  }

  /* skip whitespaces */
  while (isspace(*plugin_name)) {
    plugin_name++;
  }

  plugin = olsr_plugins_get(plugin_name);
  if (str_hasnextword(data->parameter, "load") == NULL) {
    if (plugin != NULL) {
      abuf_appendf(data->out, "Plugin %s already loaded\n", plugin_name);
      return TELNET_RESULT_ACTIVE;
    }
    plugin = olsr_plugins_load(plugin_name);
    if (plugin != NULL) {
      abuf_appendf(data->out, "Plugin %s successfully loaded\n", plugin_name);
    }
    else {
      abuf_appendf(data->out, "Could not load plugin %s\n", plugin_name);
    }
    return TELNET_RESULT_ACTIVE;
  }

  if (plugin == NULL) {
    abuf_appendf(data->out, "Error, could not find plugin '%s'.\n", plugin_name);
    return TELNET_RESULT_ACTIVE;
  }

  if (str_hasnextword(data->parameter, "unload") == NULL) {
    if (olsr_plugins_unload(plugin)) {
      abuf_appendf(data->out, "Could not unload plugin %s\n", plugin_name);
    }
    else {
      abuf_appendf(data->out, "Plugin %s successfully unloaded\n", plugin_name);
    }
    return TELNET_RESULT_ACTIVE;
  }

  abuf_appendf(data->out, "Unknown command '%s %s %s'.\n",
      data->command, data->parameter, plugin_name);
  return TELNET_RESULT_ACTIVE;
}
