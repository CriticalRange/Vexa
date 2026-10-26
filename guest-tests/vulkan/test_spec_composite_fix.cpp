// Host-independent unit test for VEXA's Mali spec-constant-composite SPIR-V fix.
// Includes the thunk implementation directly and feeds it hand-assembled words.
#include <cstdio>
#include <cstdint>
#include <vector>

#include "vulkan_spirv_fixes.inc"

static int failures = 0;
#define CHECK(cond, name) do { if (!(cond)) { ++failures; printf("FAIL %s\n", name); } else { printf("PASS %s\n", name); } } while (0)

static uint32_t inst(uint32_t wc, uint32_t op) { return (wc << 16) | op; }

int main() {
  // (a) OpConstantComposite with an OpSpecConstantTrue constituent -> converted.
  {
    std::vector<uint32_t> m = {
      0x07230203, 0x00010000, 0, 32, 0,
      inst(3, 48), 2, 10,        // %10 = OpSpecConstantTrue %2
      inst(4, 44), 3, 11, 10,    // %11 = OpConstantComposite %3 %10
    };
    CHECK(VexaNeedsSpecCompositeFix(m.data(), m.size()), "a: pre-scan true");
    CHECK(VexaFixSpecConstantComposites(m) == 1, "a: one fixed");
    CHECK((m[8] & 0xffff) == 51, "a: opcode now OpSpecConstantComposite");
  }

  // (b) Composite of a plain OpConstant -> unchanged, pre-scan false.
  {
    std::vector<uint32_t> m = {
      0x07230203, 0x00010000, 0, 32, 0,
      inst(4, 43), 2, 10, 42,    // %10 = OpConstant %2 42
      inst(4, 44), 3, 11, 10,    // %11 = OpConstantComposite %3 %10
    };
    CHECK(!VexaNeedsSpecCompositeFix(m.data(), m.size()), "b: pre-scan false");
    CHECK(VexaFixSpecConstantComposites(m) == 0, "b: none fixed");
    CHECK((m[9] & 0xffff) == 44, "b: opcode unchanged");
  }

  // (c) Composite B referencing converted composite A -> also converted.
  {
    std::vector<uint32_t> m = {
      0x07230203, 0x00010000, 0, 32, 0,
      inst(3, 48), 2, 10,        // %10 = OpSpecConstantTrue %2
      inst(4, 44), 3, 11, 10,    // %11 = OpConstantComposite %3 %10
      inst(4, 44), 3, 12, 11,    // %12 = OpConstantComposite %3 %11
    };
    CHECK(VexaNeedsSpecCompositeFix(m.data(), m.size()), "c: pre-scan true");
    CHECK(VexaFixSpecConstantComposites(m) == 2, "c: fixpoint converts both");
    CHECK((m[8] & 0xffff) == 51 && (m[12] & 0xffff) == 51, "c: opcodes fixed");
  }

  // (d) Malformed input -> 0, no crash.
  {
    std::vector<uint32_t> trunc = {0x07230203, 0x00010000, 0, 32, 0, inst(4, 44), 3, 11};
    CHECK(VexaFixSpecConstantComposites(trunc) == 0, "d: truncated instruction");
    std::vector<uint32_t> zerowc = {0x07230203, 0x00010000, 0, 32, 0, inst(0, 44), 3, 11, 10};
    CHECK(VexaFixSpecConstantComposites(zerowc) == 0, "d: zero word count");
    std::vector<uint32_t> badmagic = {0xdeadbeef, 0, 0, 32, 0};
    CHECK(!VexaNeedsSpecCompositeFix(badmagic.data(), badmagic.size()), "d: bad magic");
    std::vector<uint32_t> tiny = {0x07230203, 0};
    CHECK(!VexaNeedsSpecCompositeFix(tiny.data(), tiny.size()), "d: too short");
  }

  printf("%s: %d failure(s)\n", failures ? "FAILED" : "PASSED", failures);
  return failures ? 1 : 0;
}
