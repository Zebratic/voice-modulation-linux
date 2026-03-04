#include <QTest>
#include "audio/RingBuffer.h"

class TestRingBuffer : public QObject {
    Q_OBJECT

private slots:
    void testBasicWriteRead() {
        RingBuffer rb(1024);
        float data[] = {1.f, 2.f, 3.f, 4.f};
        QCOMPARE(rb.write(data, 4), size_t(4));
        QCOMPARE(rb.availableRead(), size_t(4));

        float out[4] = {};
        QCOMPARE(rb.read(out, 4), size_t(4));
        QCOMPARE(out[0], 1.f);
        QCOMPARE(out[3], 4.f);
        QCOMPARE(rb.availableRead(), size_t(0));
    }

    void testOverflow() {
        RingBuffer rb(4);
        float data[] = {1.f, 2.f, 3.f, 4.f, 5.f};
        QCOMPARE(rb.write(data, 5), size_t(4));
        QCOMPARE(rb.availableRead(), size_t(4));
    }

    void testWrapAround() {
        RingBuffer rb(4);
        float d1[] = {1.f, 2.f, 3.f};
        rb.write(d1, 3);
        float tmp[3];
        rb.read(tmp, 3);

        float d2[] = {4.f, 5.f, 6.f};
        QCOMPARE(rb.write(d2, 3), size_t(3));

        float out[3] = {};
        QCOMPARE(rb.read(out, 3), size_t(3));
        QCOMPARE(out[0], 4.f);
        QCOMPARE(out[1], 5.f);
        QCOMPARE(out[2], 6.f);
    }

    void testReset() {
        RingBuffer rb(8);
        float data[] = {1.f, 2.f};
        rb.write(data, 2);
        QCOMPARE(rb.availableRead(), size_t(2));
        rb.reset();
        QCOMPARE(rb.availableRead(), size_t(0));
    }
};

QTEST_MAIN(TestRingBuffer)
#include "test_ring_buffer.moc"
