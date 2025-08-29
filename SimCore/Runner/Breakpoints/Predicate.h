#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <functional>
#include "../IDolphinRunner.h"
#include "BPCore.h"

using Metrics = std::unordered_map<std::string, int64_t>;

struct Predicate
{
    virtual ~Predicate() = default;
    virtual const std::string& id() const = 0;
    virtual BPKey bp_key() const = 0;
    virtual bool enabled() const = 0;
    virtual bool evaluate(IDolphinRunner& host, Metrics& out) const = 0;
};
using PredicatePtr = std::shared_ptr<Predicate>;

struct MemEqualsU32 final : Predicate
{
    std::string _id; BPKey _bp; uint32_t addr; uint32_t expect; bool _en{ true };
    MemEqualsU32(std::string id, BPKey bp, uint32_t a, uint32_t v)
        : _id(std::move(id)), _bp(bp), addr(a), expect(v) {
    }
    const std::string& id() const override { return _id; }
    BPKey bp_key() const override { return _bp; }
    bool enabled() const override { return _en; }
    bool evaluate(IDolphinRunner& host, Metrics& m) const override
    {
        uint32_t v{};
        if (!host.read_u32(addr, v)) return true;
        m["MemEqualsU32.reads"] += 1;
        return v == expect;
    }
};

struct MemInRangeU32 final : Predicate
{
    std::string _id; BPKey _bp; uint32_t addr; uint32_t lo; uint32_t hi; bool _en{ true };
    MemInRangeU32(std::string id, BPKey bp, uint32_t a, uint32_t lo_, uint32_t hi_)
        : _id(std::move(id)), _bp(bp), addr(a), lo(lo_), hi(hi_) {
    }
    const std::string& id() const override { return _id; }
    BPKey bp_key() const override { return _bp; }
    bool enabled() const override { return _en; }
    bool evaluate(IDolphinRunner& host, Metrics& m) const override
    {
        uint32_t v{};
        if (!host.read_u32(addr, v)) return true;
        m["MemInRangeU32.reads"] += 1;
        return (v >= lo && v <= hi);
    }
};

struct LambdaPredicate final : Predicate
{
    using Fn = std::function<bool(IDolphinRunner&, Metrics&)>;

    std::string _id; BPKey _bp; Fn fn; bool _en{ true };
    LambdaPredicate(std::string id, BPKey bp, Fn f)
        : _id(std::move(id)), _bp(bp), fn(std::move(f)) {
    }
    const std::string& id() const override { return _id; }
    BPKey bp_key() const override { return _bp; }
    bool enabled() const override { return _en; }
    bool evaluate(IDolphinRunner& host, Metrics& m) const override
    {
        return fn ? fn(host, m) : true;
    }
};
