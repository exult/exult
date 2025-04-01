/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * This file is for all the operation to work with linked lists
 * linked lists are used to store what function needs to be applied to each
 * colour and also for keeping track of which plugin is loaded
 */

#include "linked.h"

#include "globals.h"
#include "plugin.h"

#include <stdio.h>
#include <stdlib.h>

node* create_node(void) {
	// return NULL in case of failure - prints error message
	// otherwise return address of newly created node

	node* new_node;
	if ((new_node = (node*)malloc(sizeof(node))) == NULL) {
		fprintf(stderr, "ERROR: Couldn't allocate memory");
		return NULL;
	}
	new_node->plugin_apply = NULL;
	new_node->next         = NULL;
	new_node->handle       = NULL;

	if (g_statics.debug > 2) {
		printf("node created\n");
		fflush(stdout);
	}
	return new_node;
}

int delete_node(node* n) {
	// for now, returns 0 in case of error
	// for now, returns (!0) otherwise

	if (n != NULL) {
		int val = 1;
		if (n->handle != NULL) {
			// that's the handle list. We must use plug_unload
			// note: plug_unload return non-zero when successful!
			// must call deinit first
			void* (*deinit)(void);
			*(void**)&deinit = plug_load_func(n->handle, "deinit_plugin");
			(*deinit)();
			val = plug_unload(n->handle);
		}
		free(n);
		return val;
	} else {
		// n == NULL!!!!!! You silly boy!!!
		return -1;
	}
}

int delete_list(node* list_head) {
	node* cursor = list_head;
	while (cursor != NULL) {
		list_head = cursor->next;
		if (delete_node(cursor) < 0) {
			fprintf(stderr, "ERROR: problem with delete node!\n");
			return -1;
		}    // some problem
		if (g_statics.debug > 2) {
			printf("node deleted\n");
			fflush(stdout);
		}
		cursor = list_head;
	}
	return 0;
}

int add_tail_list(node* list_head, node* new_node) {
	node* cursor = list_head;
	if (cursor == NULL) {
		list_head = new_node;
	} else {
		while (cursor->next != NULL) {
			cursor = cursor->next;
		}
		// cursor->next == NULL
		cursor->next = new_node;
	}
	return 1;
}

node* add_head_list(node* list_head, node* new_node) {
	// return the address of the start of the new list.
	// this should be called this way: list_head =
	// add_head_list(list_head,my_new_node);
	new_node->next = list_head;
	return new_node;
}
