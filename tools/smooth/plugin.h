/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 */

char*       plug_error(void);
void*       plug_load_func(libhandle_t a_hdl, const char* func_name);
libhandle_t plug_load(char* plug_name);
int         plug_unload(libhandle_t a_hdl);
int         add_plugin_apply(int col_index, libhandle_t a_hdl);
int         add_plugin_parse(char* line, libhandle_t a_hdl);
node*       add_handle(libhandle_t a_hdl, node* list);
