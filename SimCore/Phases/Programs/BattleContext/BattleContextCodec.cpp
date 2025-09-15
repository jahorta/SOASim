#include "BattleContextCodec.h"
#include "../../../Core/Memory/SoaAddrRegistry.h"
#include "../../../Core/Memory/SoaStructReaders.h"
#include <cstring>

namespace simcore::battlectx::codec {

    bool extract_from_mem1(const simcore::MemView& view, BattleContext& out)
    {
        if (!view.valid()) return false;

        for (int i = 0; i < 12; ++i) {
            out.slots[i] = {};
            out.slots[i].is_player = (i < 4) ? 1 : 0;
        }

        for (int i = 0; i < 12; ++i) {
            uint32_t p = 0;
            if (!view.read_u32(addr::Registry::spec(addr::battle::CombatantInstancesTable).base + i * 4, p)) return false;
            if (p && view.in_mem1(p)) {
                out.slots[i].present = 1;
                out.slots[i].instance_addr = p;
                (void)soa::readers::read(view, p, out.slots[i].instance);
            }
        }

        for (int i = 0; i < 12; ++i) {
            uint16_t id = 0;
            if (!view.read_u16(addr::Registry::spec(addr::battle::CombatantIdTable).base + i * 2, id)) return false;
            out.slots[i].id = id;
        }

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
            (void)soa::readers::read(view, ed_va, s.enemy_def);
        }

        return true;
    }

    // Wire format is now native-endian.
    // Per-slot: present(1), is_player(1), id(u16 native), has_enemy_def(1), instance_addr(u32 native),
    //           if present: raw sizeof(CombatantInstance),
    //           enemy_def_addr(u32 native),
    //           if has_enemy_def: raw sizeof(EnemyDefinition).

    static inline void append_u16_native(std::string& s, uint16_t v) {
        const char* p = reinterpret_cast<const char*>(&v);
        s.append(p, p + sizeof(uint16_t));
    }
    static inline void append_u32_native(std::string& s, uint32_t v) {
        const char* p = reinterpret_cast<const char*>(&v);
        s.append(p, p + sizeof(uint32_t));
    }
    static inline bool read_u16_native(const char*& p, const char* end, uint16_t& v) {
        if (p + sizeof(uint16_t) > end) return false;
        std::memcpy(&v, p, sizeof(uint16_t)); p += sizeof(uint16_t); return true;
    }
    static inline bool read_u32_native(const char*& p, const char* end, uint32_t& v) {
        if (p + sizeof(uint32_t) > end) return false;
        std::memcpy(&v, p, sizeof(uint32_t)); p += sizeof(uint32_t); return true;
    }

    bool encode(const BattleContext& in, std::string& out)
    {
        out.clear();
        out.reserve(12 * (2 + 2 + 1 + 4 + sizeof(soa::CombatantInstance) + 4 + sizeof(soa::EnemyDefinition)));

        for (int i = 0; i < 12; ++i) {
            const auto& s = in.slots[i];
            out.push_back(char(s.present));
            out.push_back(char(s.is_player));
            append_u16_native(out, s.id);
            out.push_back(char(s.has_enemy_def));
            append_u32_native(out, s.instance_addr);
            if (s.present) {
                const char* bytes = reinterpret_cast<const char*>(&s.instance);
                out.append(bytes, bytes + sizeof(soa::CombatantInstance));
            }
            append_u32_native(out, s.enemy_def_addr);
            if (s.has_enemy_def) {
                const char* bytes = reinterpret_cast<const char*>(&s.enemy_def);
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
            auto& s = out.slots[i];
            s = {};

            if (p + 1 > end) return false; s.present = static_cast<uint8_t>(*p++);
            if (p + 1 > end) return false; s.is_player = static_cast<uint8_t>(*p++);
            if (!read_u16_native(p, end, s.id)) return false;
            if (p + 1 > end) return false; s.has_enemy_def = static_cast<uint8_t>(*p++);
            if (!read_u32_native(p, end, s.instance_addr)) return false;

            if (s.present) {
                if (p + sizeof(soa::CombatantInstance) > end) return false;
                std::memcpy(&s.instance, p, sizeof(soa::CombatantInstance));
                p += sizeof(soa::CombatantInstance);
            }

            if (!read_u32_native(p, end, s.enemy_def_addr)) return false;

            if (s.has_enemy_def) {
                if (p + sizeof(soa::EnemyDefinition) > end) return false;
                std::memcpy(&s.enemy_def, p, sizeof(soa::EnemyDefinition));
                p += sizeof(soa::EnemyDefinition);
            }
        }
        return p == end;
    }

} // namespace simcore::battlectx::codec
