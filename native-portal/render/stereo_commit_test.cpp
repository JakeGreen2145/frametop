// SPDX-License-Identifier: MIT
#include "continuous_test.cpp"
#include <filesystem>
#include <cstdlib>

static void second_pipeline_failure_is_atomic() {
    for (int failed : {0, 1}) {
        Fixture f; f.start(); f.stereo();
        f.fake.fail_pipeline_eye = failed;
        assert(f.draw(0).decision == Decision::Drop);
        assert(f.draw(1).decision == Decision::Drop);
        assert(!f.adapter.state().committed && !f.adapter.state().cpu_done);
    }
}
static void required_pair_preparation() {
    for (unsigned mode = 0; mode < 15; ++mode) {
        Fixture f; f.start(); f.stereo();
        auto input = pair_input(f.fake, f.context(), f.lookup());
        switch (mode) {
        case 0: input.complete_scan = false; break;
        case 1: input.eyes[1].ready = false; break;
        case 2: input.eyes[0].ready = false; break;
        case 3: input.eyes[1] = input.eyes[0]; break;
        case 4: input.eyes[1].record = {}; break;
        case 5: ++input.command; break;
        case 6: ++input.eyes[1].context.recording_generation; break;
        case 7: ++input.eyes[1].context.identity.device_generation; break;
        case 8: input.eyes[1].record.label[0] = 'X'; break;
        case 9: ++input.eyes[1].record.descriptors[4].offset; break;
        case 10: input.eyes[1].record.prefix[200] ^= std::byte{1}; break;
        case 11: input.eyes[1].pass = f.fake.obs.arena.pass_end; break;
        case 12: ++input.eyes[1].pass; break;
        case 13: input.eyes[1].lookup.target = 0; break;
        case 14: ++input.eyes[1].lookup.x[0]; break;
        }
        assert(!f.adapter.prepare(input));
        assert(!f.fake.lookups); // both records before ANY pipeline callback
        assert(!f.adapter.commit(1));
        assert(f.adapter.backend(f.fake.records[0], f.context(), f.lookup()).decision == Decision::Drop);
        assert(f.adapter.backend(f.fake.records[1], f.context(), f.lookup()).decision == Decision::Drop);
    }
    Fixture f; f.start(); f.stereo();
    assert(f.adapter.backend(f.fake.records[0], f.context(), f.lookup()).decision == Decision::Drop);
    assert(!f.fake.lookups);
}
static void revoke(Fixture &f, unsigned kind) {
    if (kind == 0) f.adapter.off();
    if (kind == 1) now = f.frame.lease.deadline_ns;
    if (kind == 2) f.control.invalidate(Reason::External);
    if (kind == 3) f.control.invalidate(Reason::Off);
}
static void commit_boundaries() {
    for (unsigned order = 0; order < 2; ++order)
        for (unsigned kind = 0; kind < 4; ++kind)
            for (unsigned boundary = 0; boundary < 5; ++boundary) {
                Fixture f; f.start();
                assert(f.adapter.enqueue(f.eye(order)));
                assert(f.adapter.enqueue(f.eye(1-order)));
                assert(f.adapter.outer_return());
                auto input = pair_input(f.fake, f.context(), f.lookup());
                if (order) std::swap(input.eyes[0], input.eyes[1]);
                if (boundary == 0) revoke(f, kind);
                const auto generation = f.adapter.prepare(input);
                if (boundary == 0) { assert(!generation); continue; }
                assert(generation && f.fake.lookups == 2);
                if (boundary == 1) revoke(f, kind);
                const bool committed = f.adapter.commit(generation);
                if (boundary == 1) { assert(!committed); continue; }
                assert(committed && f.adapter.state().committed);
                if (boundary == 2) revoke(f, kind);
                assert(f.adapter.backend(f.fake.records[order], f.context(), f.lookup()).decision == Decision::Portal);
                if (boundary == 3) revoke(f, kind);
                Recorded stock; stock.prefix = f.eye(0).prefix;
                assert(f.adapter.backend(stock, {}, f.lookup()).decision == Decision::Stock);
                assert(f.adapter.backend(f.fake.records[1-order], f.context(), f.lookup()).decision == Decision::Portal);
                if (boundary == 4) revoke(f, kind);
                assert(f.adapter.state().cpu_done && f.fake.lookups == 3);
                assert(!f.adapter.commit(generation));
                assert(!f.adapter.prepare(input));
                assert(f.adapter.state().pending);
                assert(!f.adapter.submitted()); // R1: save success without revoked publication
                assert(f.adapter.state().stereo_submitted);
                f.adapter.off();
                BackendStatus s; assert(f.control.status(s) && s.pending);
                assert(!f.adapter.poll_completion());
                f.fake.complete = true;
                assert(f.adapter.poll_completion());
                assert(f.control.status(s) && s.pending); // GPU alone is not disposal
                assert(!f.adapter.reclaim());
            }
}
static Fixture *callback_fixture = nullptr;
static unsigned callback_stage = 0, callback_fault = 0;
static void preparation_callback_boundaries() {
    for (unsigned stage = 0; stage < 2; ++stage)
        for (unsigned kind = 0; kind < 5; ++kind) {
            Fixture f; f.start(); f.stereo();
            callback_fixture = &f; callback_stage = stage; callback_fault = kind;
            f.fake.during_pipeline = +[](Fake &n, unsigned eye) {
                if (eye != callback_stage) return;
                if (callback_fault == 0) now = 100;
                if (callback_fault == 1) n.valid = false;
                if (callback_fault == 2) ++n.obs.arena.recording_generation;
                if (callback_fault == 3) callback_fixture->control.invalidate(Reason::Off);
                if (callback_fault == 4) callback_fixture->control.invalidate(Reason::External);
            };
            assert(!f.adapter.prepare(pair_input(f.fake, f.context(), f.lookup())));
            assert(f.adapter.backend(f.fake.records[0], f.context(), f.lookup()).decision == Decision::Drop);
            assert(f.adapter.backend(f.fake.records[1], f.context(), f.lookup()).decision == Decision::Drop);
        }
    callback_fixture = nullptr;
}
static void committed_authentication() {
    for (unsigned mode = 0; mode < 7; ++mode) {
        Fixture f; f.start(); f.stereo();
        assert(prepare_commit(f.adapter, f.fake, f.context(), f.lookup()));
        const auto calls = f.fake.lookups;
        auto rec = f.fake.records[1]; auto ctx = f.context(); auto lookup = f.lookup();
        if (mode == 0) rec.label[0] = 'X';
        if (mode == 1) ++rec.descriptors[0].range;
        if (mode == 2) ++rec.descriptors[8].buffer;
        if (mode == 3) ++ctx.recording_generation;
        if (mode == 4) ++ctx.identity.renderer_generation;
        if (mode == 5) ++lookup.x[2];
        if (mode == 6) ++lookup.target;
        assert(f.adapter.backend(rec, ctx, lookup).decision == Decision::Drop);
        assert(f.adapter.backend(f.fake.records[0], f.context(), f.lookup()).decision == Decision::Portal);
        assert(f.adapter.backend(f.fake.records[0], f.context(), f.lookup()).decision == Decision::Drop);
        assert(f.adapter.backend(f.fake.records[1], f.context(), f.lookup()).decision == Decision::Portal);
        assert(f.fake.lookups == calls);
    }
    for (unsigned phase = 0; phase < 2; ++phase) {
        Fixture f; f.start(); f.stereo();
        const auto generation = f.adapter.prepare(pair_input(f.fake, f.context(), f.lookup()));
        assert(generation);
        if (phase) assert(f.adapter.commit(generation));
        ++f.fake.obs.arena.recording_generation;
        if (!phase) assert(!f.adapter.commit(generation));
        assert(f.adapter.backend(f.fake.records[0], f.context(), f.lookup()).decision == Decision::Drop);
    }
}
static void pending_until_both_settlements() {
    ContinuousFixture f; f.begin(1); f.stereo();
    const auto generation = f.adapter.prepare(pair_input(f.native, f.context(), f.source.lookup()));
    assert(generation && f.adapter.commit(generation));
    f.adapter.off(); f.cpu(); f.queue_success();
    assert(!f.adapter.submitted());
    f.native.cpu_settled = true;
    assert(!f.adapter.reclaim());
    BackendStatus s; assert(f.source.control.status(s) && s.pending);
    f.complete(); f.native.cpu_settled = false;
    assert(!f.adapter.reclaim());
    assert(f.source.control.status(s) && s.pending);
    f.settled();
    assert(f.source.control.status(s) && !s.pending);
    assert(!f.adapter.state().pending);
    f.source.control.settled();
    f.begin(2); f.stereo();
    const auto next = f.adapter.prepare(pair_input(f.native, f.context(), f.source.lookup()));
    assert(next > generation && !f.adapter.commit(generation));
    assert(!f.adapter.commit(next));
}
static void ipc_off_drains_real_adapter() {
    // Actual private transport + Adapter + FrameGate; no screen-owner/UI fiction.
    ContinuousFixture f;
    std::string path = std::string(std::getenv("TMPDIR")) + "/sc-XXXXXX";
    assert(mkdtemp(path.data()));
    Server server(path, f.source.control, clock_now); assert(server.start());
    Client client; assert(client.connect(server.path()));
    Request q; q.sequence = 1; q.sent_ns = now;
    Reply reply; assert(client.exchange(q, reply) && reply.epoch);
    q.epoch = reply.epoch; q.origin = reply.origin; q.generation = 1;
    q.op = Operation::Update; ++q.sequence;
    assert(client.exchange(q, reply));
    PolicyInput policy; policy.enabled = true;
    f.frame = f.source.gate.begin(1, policy, true, true, false, now);
    assert(f.adapter.latch(f.frame));
    f.stereo();
    assert(prepare_commit(f.adapter, f.native, f.context(), f.source.lookup()));
    assert(f.draw(0).decision == Decision::Portal);
    q.op = Operation::Release; ++q.sequence;
    assert(client.exchange(q, reply));
    assert(reply.state == native_portal::State::Draining && "IPC Off cannot complete pending render debt");
    assert(f.draw(1).decision == Decision::Portal);
    f.queue_success(); assert(!f.adapter.submitted());
    f.complete(); ++q.sequence;
    assert(client.exchange(q, reply) && reply.state == native_portal::State::Draining);
    f.settled(); ++q.sequence;
    assert(client.exchange(q, reply) && reply.state == native_portal::State::Off);
    client.close(); server.stop(); std::filesystem::remove_all(path);
}
int main(int argc, char **argv) {
    ipc_off_drains_real_adapter();
    // Distinct per-eye results stay cached despite caller snapshot mutation and
    // later pipeline failure; no callback can fail newly after the first draw.
    {
        Fixture f; f.start(); f.stereo();
        auto input = pair_input(f.fake, f.context(), f.lookup());
        input.eyes[1].lookup.x[1] += 0x2b0;
        input.eyes[1].lookup.x[2] += 0x98;
        input.eyes[1].lookup.x[3] += 1;
        const auto left_lookup = input.eyes[0].lookup, right_lookup = input.eyes[1].lookup;
        f.fake.check_lookup = true;
        f.fake.expected_lookup = {left_lookup, right_lookup};
        const auto generation = f.adapter.prepare(input);
        assert(generation && f.fake.lookups == 2);
        input.eyes = {}; input.command = 0;
        assert(f.adapter.commit(generation));
        f.fake.pipeline_ok = false;
        assert(f.adapter.backend(f.fake.records[1], f.context(), right_lookup).pipeline == 0x778);
        f.adapter.off();
        f.gate.end(f.frame); // legacy outer cleanup cannot cancel committed authorization
        PolicyInput policy; policy.enabled = true;
        assert(!f.gate.begin(2, policy, true, true, false, now).id);
        assert(!f.adapter.latch(f.frame) && !f.adapter.enqueue(f.eye(0)));
        assert(f.adapter.backend(f.fake.records[0], f.context(), left_lookup).pipeline == 0x777);
        assert(f.fake.lookups == 2);
    }
    second_pipeline_failure_is_atomic();
    required_pair_preparation();
    commit_boundaries();
    preparation_callback_boundaries();
    committed_authentication();
    pending_until_both_settlements();
    std::puts("paired pre-encoding commit tests passed");
    return continuous_suite_main(argc, argv);
}
