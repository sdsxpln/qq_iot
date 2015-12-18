#ifndef _ring_queue_h
#define _ring_queue_h



typedef struct ring_queue
{
	void **data;
	char *flags;
	unsigned int size;
	unsigned int num;
	unsigned int head;
	unsigned int tail;

} ring_queue_t;



int ring_queue_init(ring_queue_t *queue, int buffer_size);
int ring_queue_push(ring_queue_t *queue, void * ele);
int  ring_queue_pop(ring_queue_t *queue, void **ele);


#endif  /*_ring_queue_h*/