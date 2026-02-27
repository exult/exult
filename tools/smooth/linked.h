/*
 * author: Aurelien Marchand
 * licence: GPL
 * date: 20/06/03
 *
 * This file is for all the operation to work with linked lists
 * linked lists are used to store what function needs to be applied to each
 * colour and also for keeping track of which plugin is loaded
 */

#include "globals.h"

node* create_node(void);
int   delete_node(node* n);
int   delete_list(node* list_head);
int   add_tail_list(node* list_head, node* new_node);
node* add_head_list(node* list_head, node* new_node);
