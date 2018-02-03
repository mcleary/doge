//
//  Copyright 2018 Christopher Di Bella
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
#include <catch/catch.hpp>
#include <doge/engine.hpp>
#include <doge/gl/shader_binary.hpp>
#include <doge/gl/shader_source.hpp>
#include <doge/gl/uniform.hpp>
#include <experimental/ranges/algorithm>
#include <experimental/ranges/concepts>
#include <experimental/ranges/functional>
#include <string_view>

namespace ranges = std::experimental::ranges;

namespace {
   template <typename T>
   struct uniform {
      std::string_view name;
      T value;
   };

   constexpr bool identity(const bool b) noexcept
   {
      return b;
   }
} // namespace <anonymous>

template <typename A, typename B, typename T1, typename T2, typename F>
void check_binary_arithmetic_operation_impl(const F& f, const T1& a, const T2& b) noexcept
{
   const auto expected = ranges::invoke(f, static_cast<A>(a), static_cast<B>(b));
   const auto actual = ranges::invoke(f, a, b);
   CHECK(expected == actual);
}

template <typename T1, typename T2, ranges::RegularInvocable<T1, T2> F>
// requires
//    RingWith<T1, T2>
void check_binary_arithmetic_operation(const F& f, const doge::uniform<T1>& a, const T2& b) noexcept
{
   check_binary_arithmetic_operation_impl<T1, T2>(f, a, b);
   check_binary_arithmetic_operation_impl<T2, T1>(f, b, a);
}

template <typename T1, typename T2, ranges::RegularInvocable<T1, T2> F>
// requires
//    RingWith<T1, T2>
void check_binary_arithmetic_operation(const F& f, const T1& a, const doge::uniform<T2>& b) noexcept
{
   check_binary_arithmetic_operation_impl<T1, T2>(f, a, b);
   check_binary_arithmetic_operation_impl<T2, T1>(f, b, a);
}

template <typename T1, typename T2, ranges::RegularInvocable<T1, T2> F>
// requires
//    RingWith<T1, T2>
void check_binary_arithmetic_operation(const F& f, const doge::uniform<T1>& a,
   const doge::uniform<T2>& b) noexcept
{
   check_binary_arithmetic_operation_impl<T1, T2>(f, a, b);
   check_binary_arithmetic_operation_impl<T2, T1>(f, b, a);
}

template <typename T>
void check_is_same_on_device(const doge::uniform<T>& u, const doge::shader_binary& program,
   const std::string_view name) noexcept
{
   const auto device = doge::uniform<const T>{program, name};
   CHECK(u == device);
}

template <typename T1, typename T2>
void check_compound_assignment(doge::uniform<T1>& a, const T2& b,
   const doge::shader_binary& program, const uniform<T1>& first)
{
   {
      const auto expected = a + b;
      a += b;
      CHECK(a == expected);
      check_is_same_on_device(a, program, first.name);
   }
   {
      const auto expected = a - b;
      a -= b;
      CHECK(a == expected);
      check_is_same_on_device(a, program, first.name);
   }
   {
      const auto expected = a * b;
      a *= b;
      CHECK(a == expected);
      check_is_same_on_device(a, program, first.name);
   }
   {
      const auto expected = a / b;
      a /= b;
      CHECK(a == expected);
      check_is_same_on_device(a, program, first.name);
   }

   if constexpr (std::is_integral_v<T1> ) {
      const auto expected = a % b;
      a %= b;
      CHECK(a == expected);
      check_is_same_on_device(a, program, first.name);
   }
}

template <typename T>
auto check_constructor(const doge::shader_binary& program, const ::uniform<T>& u)
{
   auto a = doge::uniform<T>{program, u.name, u.value};
   auto b = doge::uniform{program, u.name, u.value};

   static_assert(std::is_same_v<decltype(a), decltype(b)>);
   CHECK(static_cast<T>(a) == static_cast<T>(b));

   auto c = doge::uniform<const T>{program, u.name};
   CHECK(static_cast<T>(a) == static_cast<T>(c));

   CHECK_THROWS_AS((doge::uniform<T>{program, "dne", u.value}), doge::uniform_not_found);
   CHECK_THROWS_AS((doge::uniform<const T>{program, "dne"}), doge::uniform_not_found);

   CHECK_THROWS_AS((doge::uniform<T>{program, "bad_type", u.value}), std::runtime_error);

   return std::make_pair(std::move(a), std::move(c));
}

template <typename T1, typename T2, typename T3, typename T4>
void check_equivalent(const T1& eq1, const T2& eq2, const T3& eq3, const T4& neq) noexcept
{
   // test reflexivity on operator==
   CHECK(eq1 == eq1);

   // test symmetry on operator==
   CHECK(eq1 == eq2);
   CHECK(not (eq1 == neq));

   CHECK(eq2 == eq1);
   CHECK(not (neq == eq1));

   // test transitivity on operator==
   CHECK(eq2 == eq3);
   CHECK(not (eq2 == neq));
   CHECK(eq1 == eq3);
   CHECK(not (neq == eq3));

   // test anti-reflexivity on operator!=
   CHECK(not (eq1 != eq1)); 

   // test symmetry on operator!=
   CHECK(eq1 != neq);
   CHECK(not (eq1 != eq2));

   CHECK(neq != eq1);
   CHECK(not (eq2 != eq1));

   // test transitivity on operator!=
   CHECK(eq2 != neq);
   CHECK(not (eq2 != eq3));

   CHECK(eq3 != neq);
   CHECK(not (eq1 != eq3));
}

template <typename T1, typename T2, typename T3, typename T4, typename T5>
void check_strict_total_order(const T1& low1, const T2& low2, const T3& low3, const T4& mid,
   const T5& high) noexcept
{
   const auto check_strict_order = [](auto f, const auto& a, const auto& b,
      const auto& c) noexcept {
      if (not std::is_same_v<decltype(f), std::less_equal<>> &&
         not std::is_same_v<decltype(f), std::greater_equal<>>) {
         // check anti-reflexivity
         CHECK(not ranges::invoke(f, a, a));
      }

      // check anti-symmetry
      CHECK(ranges::invoke(f, a, b));
      CHECK(not ranges::invoke(f, b, a));

      // check transitivity
      CHECK(ranges::invoke(f, b, c));
      CHECK(ranges::invoke(f, a, c));
   };

   check_strict_order(std::less<>{}, low1, mid, high);
   check_strict_order(std::less_equal<>{}, low1, mid, high);
   check_strict_order(std::greater_equal<>{}, high, mid, low1);
   check_strict_order(std::greater<>{}, high, mid, low1);

   const auto check_equivalent = [](const auto& f, const auto& a, const auto& b,
      const auto& c) noexcept {
      // reflexivity
      CHECK(ranges::invoke(f, a, a));

      // symmetry
      CHECK(ranges::invoke(f, a, b));
      CHECK(ranges::invoke(f, b, a));

      // transitivity
      CHECK(ranges::invoke(f, b, c));
      CHECK(ranges::invoke(f, a, c));
   };

   check_equivalent(std::less_equal<>{}, low1, low2, low3);
   check_equivalent(std::greater_equal<>{}, low1, low2, low3);
}

template <typename T>
void check_arithmetic(doge::uniform<T>& a, const doge::shader_binary& program,
   const uniform<T> first, const uniform<T> second) noexcept
{
   a = first.value + first.value;
   const auto b = doge::uniform<const T>{program, first.name};
   const auto n = []{
      if constexpr (doge::is_one_of_v<T, GLfloat, GLint, GLuint>) {
         return T{10};
      }
      else if constexpr (doge::detail::is_vec2<T>) {
         return T{10, 10};
      }
      else if constexpr (doge::detail::is_vec3<T>) {
         return T{10, 10, 10};
      }
      else if constexpr (doge::detail::is_vec4<T>) {
         return T{10, 10, 10, 10};
      }
   }();
   {
      CHECK(+a == static_cast<T>(a));
      CHECK(+b == static_cast<T>(b));
      CHECK(-a == -static_cast<T>(a));
      CHECK(-b == -static_cast<T>(b));
   }
   {
      auto expected = static_cast<T>(a);
      CHECK(++a == ++expected);
      CHECK(a++ == expected++);
      CHECK(a == expected); // make sure the result actually increased.

      CHECK(--a == --expected);
      CHECK(a-- == expected--);
      CHECK(a == expected); // make sure the result actually decreased.
   }

   check_compound_assignment(a, doge::uniform{program, second.name, second.value}, program, first);
   check_compound_assignment(a, doge::uniform<const T>{program, second.name}, program, first);
   check_compound_assignment(a, n, program, first);

   auto check_binary_arithmetic = [&](const auto& f) {
      check_binary_arithmetic_operation(f, a, n);
      check_binary_arithmetic_operation(f, a, a);
      check_binary_arithmetic_operation(f, a, b);
   };

   check_binary_arithmetic(std::plus<>{});
   check_binary_arithmetic(std::minus<>{});
   check_binary_arithmetic(std::multiplies<>{});

   if constexpr (not doge::detail::is_glm_matrix<T>) {
      check_binary_arithmetic(std::divides<>{});

      if constexpr (std::is_integral_v<T>) {
         check_binary_arithmetic(std::modulus<>{});
      }
   }
}

template <typename T>
void check_uniform(const doge::shader_binary& program, const std::array<uniform<T>, 3> u)
{
   const auto [first, second, third] = u;
   auto [a, const_a] = check_constructor(program, first);
   auto [b, const_b] = check_constructor(program, second);
   auto [c, const_c] = check_constructor(program, third);

   SECTION("[uniform.comparison]") {
      auto a_equivalent = std::array{check_constructor(program, first),
         check_constructor(program, first)};

      check_equivalent(a, a_equivalent[0].first, a_equivalent[1].first, b);
      check_equivalent(a, a_equivalent[0].second, first.value, const_b);

      if constexpr (ranges::StrictTotallyOrdered<T>) {
         check_strict_total_order(a, a_equivalent[0].first, a_equivalent[1].first, b, c);
         check_strict_total_order(a, a_equivalent[0].second, first.value, const_b, third.value);
      }
   }

   SECTION("[uniform.assignment]") {
      // test assignment
      a = second.value;
      check_is_same_on_device(a, program, first.name);

      // test equivalent to T
      CHECK(a == second.value);
   }

   SECTION("[uniform.arithmetic]") {
      check_arithmetic(a, program, first, second);
   }
}

TEST_CASE("uniforms can be read and written to", "[uniform]") {
   auto engine = doge::engine{};
   auto program = doge::shader_binary{{
      std::make_pair(doge::shader_source::vertex, "test.uniform.vert.glsl"),
      std::make_pair(doge::shader_source::fragment, "test.uniform.frag.glsl")}};

   program.use([&program]{
      SECTION("[uniform.scalar]") {
         SECTION("[uniform.scalar.GLfloat]") {
            check_uniform(program, std::array<uniform<GLfloat>, 3>{{{"f.a", 0.05f}, {"f.b", 0.5f},
               {"f.c", 5.0f}}});
         }

         SECTION("[uniform.scalar.GLint]") {
            check_uniform(program, std::array<uniform<GLint>, 3>{{{"i.a", -32'767}, {"i.b", 65'536},
               {"i.c", 650'356}}});
         }

         SECTION("Testing GLuint", "[uniform.scalar.GLuint]") {
            check_uniform(program, std::array<uniform<GLuint>, 3>{{{"u.a", 15u}, {"u.b", 16u},
               {"u.c", 352u}}});
         }
      }

      SECTION("[uniform.vec2]") {
         SECTION("[uniform.vec2.GLfloat]") {
            check_uniform(program, std::array<uniform<glm::vec2>, 3>{{{"v2.a", {0.05f, 0.08f}},
               {"v2.b", {0.5f, 0.8f}}, {"v2.c", {5.0f, 8.0f}}}});
         }

         SECTION("[uniform.vec2.GLint") {
            check_uniform(program, std::array<uniform<glm::ivec2>, 3>{{{"iv2.a", {7, 20}},
               {"iv2.b", {30, 40}}, {"iv2.c", {0, 0}}}});
         }

         SECTION("[uniform.vec2.GLuint") {
            check_uniform(program, std::array<uniform<glm::uvec2>, 3>{{{"uv2.a", {10, 20}},
               {"uv2.b", {30, 40}}, {"uv2.c", {0, 0}}}});
         }
      }

      SECTION("[uniform.vec3]") {
         SECTION("[uniform.vec3.GLfloat]") {
            check_uniform(program, std::array<uniform<glm::vec3>, 3>{{{"v3.a", {0.05f, 0.08f, 0.02f}},
               {"v3.b", {0.5f, 0.8f, 0.2f}}, {"v3.c", {5.0f, 8.0f, 2.0f}}}});
         }

         SECTION("[uniform.vec3.GLint") {
            check_uniform(program, std::array<uniform<glm::ivec3>, 3>{{{"iv3.a", {7, 20, 123}},
               {"iv3.b", {30, 40, 5342}}, {"iv3.c", {0, 0, 0}}}});
         }

         SECTION("[uniform.vec3.GLuint") {
            check_uniform(program, std::array<uniform<glm::uvec3>, 3>{{{"uv3.a", {10, 20, 123}},
               {"uv3.b", {30, 40, 5342}}, {"uv3.c", {0, 0, 0}}}});
         }
      }

      SECTION("[uniform.vec4]") {
         SECTION("[uniform.vec4.GLfloat]") {
            check_uniform(program, std::array<uniform<glm::vec4>, 3>{{{"v4.a", {0.05f, 0.08f, 0.02f, 0.06f}},
               {"v4.b", {0.5f, 0.8f, 0.2f, 0.6f}}, {"v4.c", {5.0f, 8.0f, 2.0f, 6.0f}}}});
         }

         SECTION("[uniform.vec4.GLint") {
            check_uniform(program, std::array<uniform<glm::ivec4>, 3>{{{"iv4.a", {7, 20, 123, 42}},
               {"iv4.b", {30, 40, 5342, 135}}, {"iv4.c", {0, 0, 0, 0}}}});
         }

         SECTION("[uniform.vec4.GLuint") {
            check_uniform(program, std::array<uniform<glm::uvec4>, 3>{{{"uv4.a", {10, 20, 123, 21}},
               {"uv4.b", {30, 40, 5342, 32}}, {"uv4.c", {0, 0, 0, 0}}}});
         }
      }
   // SECTION("Testing 2xN matrix", "[uniform.mat2xN]") {}
   // SECTION("Testing Nx2 matrix", "[uniform.matNx2]") {}
   // SECTION("Testing 3xN matrix", "[uniform.mat3xN]") {}
   // SECTION("Testing Nx3 matrix", "[uniform.matNx3]") {}
   // SECTION("Testing 4xN matrix", "[uniform.mat4xN]") {}
   // SECTION("Testing Nx4 matrix", "[uniform.matNx4]") {}
   });
}