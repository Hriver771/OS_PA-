#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "smalloc.h" 

sm_container_ptr sm_first = 0x0 ;
sm_container_ptr sm_last = 0x0 ;
sm_container_ptr sm_unused_containers = 0x0 ;

void sm_ist_unused_list(sm_container_ptr hole)
{
	printf("########insert: %p\n", hole->data) ;
	sm_container_ptr tmp = sm_unused_containers ;
	if (tmp == NULL)
	{
		hole->next_unused = NULL ;
		sm_unused_containers = hole ;
		print_unused_list() ;
		return ;
	}
	if (tmp == hole)
		return ;
	while (tmp->next_unused != NULL)
	{
		if(tmp->next_unused == hole)
			return ;
		tmp = tmp->next_unused ;		
	}
	tmp->next_unused = hole;
	hole->next_unused = NULL ;
	print_unused_list() ;
}

void sm_del_unused_list(sm_container_ptr rm)
{
	printf("@@@@@@@@@@@@ del:%p @@@@@@@@@@@\n", rm->data);
	sm_container_ptr tmp = sm_unused_containers ;

	if (tmp == NULL)
	{
		fprintf(stderr, "ERROR: del_unused_list : EMPTY\n");
		return ;
	}
	if (tmp == rm)
	{
		sm_unused_containers = rm->next_unused ;
		rm->next_unused = NULL ;
		print_unused_list() ;
		return ;
	}
	while (tmp->next_unused != NULL)
	{
		if(tmp->next_unused == rm)
			break ;
		tmp = tmp->next_unused ;
	}
	if (tmp->next_unused != rm)
	{
		fprintf(stderr, "ERROR: del_unused_list : No such a link\n") ;
		return ;
	}
	tmp->next_unused = rm->next_unused ;
	rm->next_unused = NULL ;
	print_unused_list() ;
}

void print_unused_list()
{
	sm_container_ptr tmp = sm_unused_containers ;
	if(tmp == NULL)
	{
		printf("There is no unused container\n") ;
		return ;
	}
	printf("%p", tmp->data) ;
	while (tmp->next_unused != NULL)
	{
		tmp = tmp->next_unused ;
		printf(" -> %p", tmp->data) ;
	}
	printf("\n");
	return ;
}

void sm_container_split(sm_container_ptr hole, size_t size)
{
	sm_ist_unused_list(hole);
	sm_container_ptr remainder = hole->data + size ;

	remainder->data = ((void *)remainder) + sizeof(sm_container_t) ;
	remainder->dsize = hole->dsize - size - sizeof(sm_container_t) ;
	remainder->status = Unused ;
	remainder->next = hole->next ;
	sm_ist_unused_list(remainder) ;
	hole->next = remainder ;

	if (hole == sm_last)
		sm_last = remainder ;
}

void * sm_retain_more_memory(int size)
{
	sm_container_ptr hole ;
	int pagesize = getpagesize() ;
	int n_pages = 0 ;

	n_pages = (sizeof(sm_container_t) + size + sizeof(sm_container_t)) / pagesize  + 1 ;
	hole = (sm_container_ptr) sbrk(n_pages * pagesize) ;
	if (hole == 0x0)
		return 0x0 ;

	hole->data = ((void *) hole) + sizeof(sm_container_t) ;
	hole->dsize = n_pages * getpagesize() - sizeof(sm_container_t) ;
	hole->status = Unused ;

	return hole ;
}

void * smalloc(size_t size) 
{
	sm_container_ptr hole = 0x0 ;

	sm_container_ptr itr = 0x0 ;
	for (itr = sm_first ; itr != 0x0 ; itr = itr->next) {
		if (itr->status == Busy)
			continue ;

		if (size == itr->dsize) {
			// a hole of the exact size
			itr->status = Busy ;
			sm_del_unused_list(itr) ;
			return itr->data ;
		}
		else if (size + sizeof(sm_container_t) < itr->dsize) {
			// a hole large enought to split 
			hole = itr ;
			break ; 
		}
	}
	if (hole == 0x0) {
		hole = sm_retain_more_memory(size) ;

		if (hole == 0x0)
			return 0x0 ;

		if (sm_first == 0x0) {
			sm_first = hole ;
			sm_last = hole ;
			hole->next = 0x0 ;
		}
		else {
			sm_last->next = hole ;
			sm_last = hole ;
			hole->next = 0x0 ;
		}
	}
	sm_container_split(hole, size) ;
	hole->dsize = size ;
	hole->status = Busy ;
	sm_del_unused_list(hole) ;
	return hole->data ;
}

void sm_merge_hole(sm_container_ptr itr)
{
	sm_container_ptr mer = itr->next ;

	sm_del_unused_list(itr) ;
	sm_del_unused_list(itr->next) ;

	itr->dsize += sizeof(sm_container_t) + mer->dsize ;
	itr->next = mer->next ;
//	free(mer) ;

	sm_ist_unused_list(itr) ;
}


void sfree(void * p)
{
	sm_container_ptr itr ;
	for (itr = sm_first ; itr->next != 0x0 ; itr = itr->next) {
		if (itr->data == p) {
			itr->status = Unused ;
			sm_ist_unused_list(itr) ;
			if ((itr->next)->status == Unused)
			{
				sm_merge_hole(itr);
			}
			break ;
		}
	}
}

void print_sm_containers()
{
	sm_container_ptr itr ;
	int i = 0 ;

	printf("==================== sm_containers ====================\n") ;
	for (itr = sm_first ; itr != 0x0 ; itr = itr->next, i++) {
		char * s ;
		printf("%3d:%p:%s:", i, itr->data, itr->status == Unused ? "Unused" : "  Busy") ;
		printf("%8d:", (int) itr->dsize) ;

		for (s = (char *) itr->data ;
			 s < (char *) itr->data + (itr->dsize > 8 ? 8 : itr->dsize) ;
			 s++) 
			printf("%02x ", *s) ;
		printf("\n") ;
	}
	printf("=======================================================\n") ;

}
