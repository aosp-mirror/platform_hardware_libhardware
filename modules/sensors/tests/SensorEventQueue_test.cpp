#include <stdio.h>
#include <stdlib.h>
#include <hardware/sensors.h>
#include <pthread.h>
#include "SensorEventQueue.cpp"

// Unit tests for the SensorEventQueue.

// Run it like this:
//
// make sensorstests -j32 && \
// out/host/linux-x86/obj/EXECUTABLES/sensorstests_intermediates/sensorstests

bool checkWritableBufferSize(SensorEventQueue* queue, int requested, int expected) {
    sensors_event_t* buffer;
    int actual = queue->getWritableRegion(requested, &buffer);
    if (actual != expected) {
        printf("Expected buffer size was %d; actual was %d\n", expected, actual);
        return false;
    }
    return true;
}

bool checkSize(SensorEventQueue* queue, int expected) {
    int actual = queue->getSize();
    if (actual != expected) {
        printf("Expected queue size was %d; actual was %d", expected, actual);
        return false;
    }
    return true;
}

bool testSimpleWriteSizeCounts() {
    printf("TEST testSimpleWriteSizeCounts\n");
    SensorEventQueue* queue = new SensorEventQueue(10);
    if (!checkSize(queue, 0)) return false;
    if (!checkWritableBufferSize(queue, 11, 10)) return false;
    if (!checkWritableBufferSize(queue, 10, 10)) return false;
    if (!checkWritableBufferSize(queue, 9, 9)) return false;

    queue->markAsWritten(7);
    if (!checkSize(queue, 7)) return false;
    if (!checkWritableBufferSize(queue, 4, 3)) return false;
    if (!checkWritableBufferSize(queue, 3, 3)) return false;
    if (!checkWritableBufferSize(queue, 2, 2)) return false;

    queue->markAsWritten(3);
    if (!checkSize(queue, 10)) return false;
    if (!checkWritableBufferSize(queue, 1, 0)) return false;

    printf("passed\n");
    return true;
}

bool testWrappingWriteSizeCounts() {
    printf("TEST testWrappingWriteSizeCounts\n");
    SensorEventQueue* queue = new SensorEventQueue(10);
    queue->markAsWritten(9);
    if (!checkSize(queue, 9)) return false;

    // dequeue from the front
    queue->dequeue();
    queue->dequeue();
    if (!checkSize(queue, 7)) return false;
    if (!checkWritableBufferSize(queue, 100, 1)) return false;

    // Write all the way to the end.
    queue->markAsWritten(1);
    if (!checkSize(queue, 8)) return false;
    // Now the two free spots in the front are available.
    if (!checkWritableBufferSize(queue, 100, 2)) return false;

    // Fill the queue again
    queue->markAsWritten(2);
    if (!checkSize(queue, 10)) return false;

    printf("passed\n");
    return true;
}

static const int TTOQ_EVENT_COUNT = 10000;

struct TaskContext {
  bool success;
  SensorEventQueue* queue;
};

void* writerTask(void* ptr) {
    printf("writerTask starts\n");
    TaskContext* ctx = (TaskContext*)ptr;
    SensorEventQueue* queue = ctx->queue;
    int totalWrites = 0;
    sensors_event_t* buffer;
    while (totalWrites < TTOQ_EVENT_COUNT) {
        queue->waitForSpaceAndLock();
        int writableSize = queue->getWritableRegion(rand() % 10 + 1, &buffer);
        queue->unlock();
        for (int i = 0; i < writableSize; i++) {
            // serialize the events
            buffer[i].timestamp = totalWrites++;
        }
        queue->lock();
        queue->markAsWritten(writableSize);
        queue->unlock();
    }
    printf("writerTask ends normally\n");
    return NULL;
}

void* readerTask(void* ptr) {
    printf("readerTask starts\n");
    TaskContext* ctx = (TaskContext*)ptr;
    SensorEventQueue* queue = ctx->queue;
    int totalReads = 0;
    while (totalReads < TTOQ_EVENT_COUNT) {
        queue->waitForDataAndLock();
        int maxReads = rand() % 20 + 1;
        int reads = 0;
        while (queue->getSize() && reads < maxReads) {
            sensors_event_t* event = queue->peek();
            if (totalReads != event->timestamp) {
                printf("FAILURE: readerTask expected timestamp %d; actual was %d\n",
                        totalReads, (int)(event->timestamp));
                ctx->success = false;
                return NULL;
            }
            queue->dequeue();
            totalReads++;
            reads++;
        }
        queue->unlock();
    }
    printf("readerTask ends normally\n");
    return NULL;
}


// Create a short queue, and write and read a ton of data through it.
// Write serial timestamps into the events, and expect to read them in the right order.
bool testTwoThreadsOneQueue() {
    printf("TEST testTwoThreadsOneQueue\n");
    SensorEventQueue* queue = new SensorEventQueue(100);

    TaskContext readerCtx;
    readerCtx.success = true;
    readerCtx.queue = queue;

    TaskContext writerCtx;
    writerCtx.success = true;
    writerCtx.queue = queue;

    pthread_t writer, reader;
    pthread_create(&reader, NULL, readerTask, &readerCtx);
    pthread_create(&writer, NULL, writerTask, &writerCtx);

    pthread_join(writer, NULL);
    pthread_join(reader, NULL);

    printf("testTwoThreadsOneQueue done\n");
    return readerCtx.success && writerCtx.success;
}


int main(int argc, char **argv) {
    if (testSimpleWriteSizeCounts() &&
            testWrappingWriteSizeCounts() &&
            testTwoThreadsOneQueue()) {
        printf("ALL PASSED\n");
    } else {
        printf("SOMETHING FAILED\n");
    }
    return EXIT_SUCCESS;
}
