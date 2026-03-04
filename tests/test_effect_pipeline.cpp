#include <QTest>
#include "audio/EffectPipeline.h"
#include "effects/EffectRegistry.h"
#include "effects/GainEffect.h"
#include <cmath>

class TestEffectPipeline : public QObject {
    Q_OBJECT

private slots:
    void testAddEffect() {
        EffectPipeline pipeline;
        pipeline.prepare(48000, 256);
        pipeline.addEffect(std::make_unique<GainEffect>());
        QCOMPARE(pipeline.effectCount(), 1);
    }

    void testProcessGain() {
        EffectPipeline pipeline;
        pipeline.prepare(48000, 256);
        auto gain = std::make_unique<GainEffect>();
        gain->params()[0].value.store(6.f); // +6 dB
        pipeline.addEffect(std::move(gain));

        float data[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        pipeline.process(data, 4);

        float expected = 0.5f * std::pow(10.f, 6.f / 20.f);
        QVERIFY(std::abs(data[0] - expected) < 0.001f);
    }

    void testRemoveEffect() {
        EffectPipeline pipeline;
        pipeline.prepare(48000, 256);
        pipeline.addEffect(std::make_unique<GainEffect>());
        pipeline.addEffect(std::make_unique<GainEffect>());
        QCOMPARE(pipeline.effectCount(), 2);
        pipeline.removeEffect(0);
        QCOMPARE(pipeline.effectCount(), 1);
    }

    void testMoveEffect() {
        EffectPipeline pipeline;
        pipeline.prepare(48000, 256);

        auto g1 = std::make_unique<GainEffect>();
        g1->params()[0].value.store(1.f);
        auto g2 = std::make_unique<GainEffect>();
        g2->params()[0].value.store(2.f);

        pipeline.addEffect(std::move(g1));
        pipeline.addEffect(std::move(g2));

        pipeline.moveEffect(0, 1);
        float val = pipeline.effectAt(1)->params()[0].value.load();
        QCOMPARE(val, 1.f);
    }

    void testBypass() {
        EffectPipeline pipeline;
        pipeline.prepare(48000, 256);
        auto gain = std::make_unique<GainEffect>();
        gain->params()[0].value.store(6.f);
        gain->setEnabled(false);
        pipeline.addEffect(std::move(gain));

        float data[4] = {0.5f, 0.5f, 0.5f, 0.5f};
        pipeline.process(data, 4);
        QCOMPARE(data[0], 0.5f); // Unchanged when bypassed
    }

    void testRegistry() {
        auto ids = EffectRegistry::instance().availableEffects();
        QVERIFY(std::find(ids.begin(), ids.end(), "gain") != ids.end());
    }
};

QTEST_MAIN(TestEffectPipeline)
#include "test_effect_pipeline.moc"
