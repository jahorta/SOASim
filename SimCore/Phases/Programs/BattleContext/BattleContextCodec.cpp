#include "BattleContextCodec.h"
#include "../../../Core/Memory/SoaAddrRegistry.h"
#include "../../../Core/Memory/SoaStructReaders.h"

namespace simcore::battlectx::codec {

    bool extract_from_mem1(const simcore::MemView& view, BattleContext& out)
    {
        if (!view.valid()) return false;

        // init slots
        for (int i = 0; i < 12; ++i) {
            out.slots[i] = {};
            out.slots[i].is_player = (i < 4) ? 1 : 0;
        }

        // instances
        for (int i = 0; i < 12; ++i) {
            uint32_t p = 0;
            if (!view.read_u32(addr::Registry::spec(addr::battle::CombatantInstancesTable).base + i * 4, p)) return false;
            if (p && view.in_mem1(p)) {
                out.slots[i].present = 1;
                out.slots[i].instance_addr = p;
                (void)soa::readers::read(view, p, out.slots[i].instance); // byte-faithful copy for now
            }
        }

        // ids
        for (int i = 0; i < 12; ++i) {
            uint16_t id = 0;
            if (!view.read_u16(addr::Registry::spec(addr::battle::CombatantIdTable).base + i * 2, id)) return false;
            out.slots[i].id = id;
        }

        // enemy definitions (PtrChain with dynamic base = instance addr)
        const auto& ed_spec = addr::Registry::spec(addr::battle::EnemyDefinitionFromInstance);
        for (int i = 4; i < 12; ++i) {
            auto& s = out.slots[i];
            if (!s.present) continue;

            uint32_t ed_va = 0;
            if (!addr::Registry::resolve_from_base(view, ed_spec, s.instance_addr, ed_va))
                continue;
            if (!view.in_mem1(ed_va)) continue;

            s.enemy_def_addr = ed_va;
            s.has_enemy_def = 1;
            (void)soa::readers::read(view, ed_va, s.enemy_def); // byte-faithful copy for now
        }

        return true;
    }

    // --- blob format (no version byte; v1 redefined):
    // For each of 12 slots in order:
    //   u8  present
    //   u8  is_player
    //   u16 id
    //   u8  has_enemy_def
    //   u32 instance_addr
    //   if present:   raw bytes of dme::CombatantInstance (sizeof(...))
    //   u32 enemy_def_addr
    //   if has_enemy_def: raw bytes of dme::EnemyDefinition (sizeof(...))

    static inline void put_u16(std::string& s, uint16_t v) {
        s.push_back(char(v >> 8));
        s.push_back(char(v & 0xFF));
    }
    static inline void put_u32(std::string& s, uint32_t v) {
        s.push_back(char((v >> 24) & 0xFF));
        s.push_back(char((v >> 16) & 0xFF));
        s.push_back(char((v >> 8) & 0xFF));
        s.push_back(char((v) & 0xFF));
    }
    static inline uint16_t get_u16(const char*& p) {
        const unsigned char b0 = static_cast<unsigned char>(*p++);
        const unsigned char b1 = static_cast<unsigned char>(*p++);
        return (uint16_t(b0) << 8) | uint16_t(b1);
    }
    static inline uint32_t get_u32(const char*& p) {
        const unsigned char b0 = static_cast<unsigned char>(*p++);
        const unsigned char b1 = static_cast<unsigned char>(*p++);
        const unsigned char b2 = static_cast<unsigned char>(*p++);
        const unsigned char b3 = static_cast<unsigned char>(*p++);
        return (uint32_t(b0) << 24) | (uint32_t(b1) << 16) | (uint32_t(b2) << 8) | uint32_t(b3);
    }

    bool encode(const BattleContext& in, std::string& out)
    {
        out.clear();
        out.reserve(12 * (2 + 2 + 1 + 4 + sizeof(soa::CombatantInstance) + 4 + sizeof(soa::EnemyDefinition)));

        for (int i = 0; i < 12; ++i) {
            const auto& s = in.slots[i];
            out.push_back(char(s.present));
            out.push_back(char(s.is_player));
            put_u16(out, s.id);
            out.push_back(char(s.has_enemy_def));
            put_u32(out, s.instance_addr);
            if (s.present) {
                const auto* bytes = reinterpret_cast<const char*>(&s.instance);
                out.append(bytes, bytes + sizeof(soa::CombatantInstance));
            }
            put_u32(out, s.enemy_def_addr);
            if (s.has_enemy_def) {
                const auto* bytes = reinterpret_cast<const char*>(&s.enemy_def);
                out.append(bytes, bytes + sizeof(soa::EnemyDefinition));
            }
        }
        return true;
    }

    bool decode(std::string_view in, BattleContext& out)
    {
        const char* p = in.data();
        const char* const end = in.data() + in.size();

        for (int i = 0; i < 12; ++i) {
            if (p + 1 > end) return false;
            auto& s = out.slots[i];
            s = {};

            s.present = static_cast<uint8_t>(*p++);
            if (p + 1 > end) return false;
            s.is_player = static_cast<uint8_t>(*p++);
            if (p + 2 > end) return false;
            s.id = get_u16(p);
            if (p + 1 > end) return false;
            s.has_enemy_def = static_cast<uint8_t>(*p++);
            if (p + 4 > end) return false;
            s.instance_addr = get_u32(p);

            if (s.present) {
                if (p + sizeof(soa::CombatantInstance) > end) return false;
                std::memcpy(&s.instance, p, sizeof(soa::CombatantInstance));
                p += sizeof(soa::CombatantInstance);
            }

            if (p + 4 > end) return false;
            s.enemy_def_addr = get_u32(p);

            if (s.has_enemy_def) {
                if (p + sizeof(soa::EnemyDefinition) > end) return false;
                std::memcpy(&s.enemy_def, p, sizeof(soa::EnemyDefinition));
                p += sizeof(soa::EnemyDefinition);
            }
        }
        return p == end;
    }

} // namespace simcore::battlectx::codec
