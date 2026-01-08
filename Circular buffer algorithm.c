#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdbool.h>

// 环形缓冲区结构体（信号量版本）
typedef struct {
    char *buffer;           // 缓冲区指针
    size_t capacity;        // 缓冲区容量
    size_t head;            // 写入位置
    size_t tail;            // 读取位置
    pthread_mutex_t mutex;  // 互斥锁（保护head/tail）
    sem_t empty_slots;      // 空槽位信号量
    sem_t filled_slots;     // 已填充槽位信号量
} RingBufferSem;

// 初始化环形缓冲区
RingBufferSem* ring_buffer_sem_create(size_t capacity) {
    RingBufferSem *rb = malloc(sizeof(RingBufferSem));
    if (!rb) return NULL;
    
    rb->buffer = malloc(capacity);
    if (!rb->buffer) {
        free(rb);
        return NULL;
    }
    
    rb->capacity = capacity;
    rb->head = 0;
    rb->tail = 0;
    
    pthread_mutex_init(&rb->mutex, NULL);
    sem_init(&rb->empty_slots, 0, capacity); // 初始时有capacity个空槽位
    sem_init(&rb->filled_slots, 0, 0);       // 初始时没有数据
    
    return rb;
}

// 释放缓冲区
void ring_buffer_sem_destroy(RingBufferSem *rb) {
    if (!rb) return;
    
    pthread_mutex_destroy(&rb->mutex);
    sem_destroy(&rb->empty_slots);
    sem_destroy(&rb->filled_slots);
    
    free(rb->buffer);
    free(rb);
}

// 写入数据（阻塞）
bool ring_buffer_sem_put(RingBufferSem *rb, const char *data, size_t len) {
    // 等待空槽位
    if (sem_wait(&rb->empty_slots) != 0) {
        return false;
    }
    
    pthread_mutex_lock(&rb->mutex);
    
    // 写入数据
    rb->buffer[rb->head] = *data;
    rb->head = (rb->head + 1) % rb->capacity;
    
    pthread_mutex_unlock(&rb->mutex);
    
    // 通知有数据可读
    sem_post(&rb->filled_slots);
    
    return true;
}

// 读取数据（阻塞）
bool ring_buffer_sem_get(RingBufferSem *rb, char *data, size_t len) {
    // 等待有数据
    if (sem_wait(&rb->filled_slots) != 0) {
        return false;
    }
    
    pthread_mutex_lock(&rb->mutex);
    
    // 读取数据
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    
    pthread_mutex_unlock(&rb->mutex);
    
    // 通知有空槽位
    sem_post(&rb->empty_slots);
    
    return true;
}

// 尝试写入（非阻塞）
bool ring_buffer_sem_try_put(RingBufferSem *rb, const char *data, size_t len) {
    // 尝试获取空槽位
    if (sem_trywait(&rb->empty_slots) != 0) {
        return false;
    }
    
    pthread_mutex_lock(&rb->mutex);
    
    rb->buffer[rb->head] = *data;
    rb->head = (rb->head + 1) % rb->capacity;
    
    pthread_mutex_unlock(&rb->mutex);
    
    sem_post(&rb->filled_slots);
    return true;
}

// 尝试读取（非阻塞）
bool ring_buffer_sem_try_get(RingBufferSem *rb, char *data, size_t len) {
    // 尝试获取数据
    if (sem_trywait(&rb->filled_slots) != 0) {
        return false;
    }
    
    pthread_mutex_lock(&rb->mutex);
    
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->capacity;
    
    pthread_mutex_unlock(&rb->mutex);
    
    sem_post(&rb->empty_slots);
    return true;
}