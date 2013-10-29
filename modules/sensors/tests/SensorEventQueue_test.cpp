#include <stdio.h>
#include <stdlib.h>
#include <hardware/sensors.h>
#include <pthread.h>
#include <cutils/atomic.h>

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

int main(int argc, char **argv) {
    if (testSimpleWriteSizeCounts() &&
            testWrappingWriteSizeCounts()) {
        printf("ALL PASSED\n");
    } else {
        printf("SOMETHING FAILED\n");
    }
    return EXIT_SUCCESS;
}
