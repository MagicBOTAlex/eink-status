#include <unity.h>

#include "ui/font.hpp"
#include "ui/param.hpp"

void setUp(void) {}
void tearDown(void) {}

namespace {

const ui::Font *null_font() { return nullptr; }

} // namespace

void test_param_types_and_getters(void) {
  const ui::Param unset;
  const ui::Param i(5);
  const ui::Param b(true);
  const ui::Param t("hello");
  const ui::Param c(ui::Color::White);
  const ui::Param a(ui::Align::Center);
  const ui::Param h(ui::HAlign::Right);
  const ui::Param v(ui::VAlign::Bottom);
  const ui::Param f(null_font());

  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::Unset, (int)unset.type());
  TEST_ASSERT_TRUE(unset.empty());
  TEST_ASSERT_EQUAL_STRING("unset", unset.dump().c_str());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::Int, (int)i.type());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::Bool, (int)b.type());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::Text, (int)t.type());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::Color, (int)c.type());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::Align, (int)a.type());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::HAlign, (int)h.type());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::VAlign, (int)v.type());
  TEST_ASSERT_EQUAL_INT((int)ui::ParamType::Font, (int)f.type());

  TEST_ASSERT_EQUAL_INT(5, i.as_int());
  TEST_ASSERT_EQUAL_INT(5, i.as_int(9)); // fallback unused when set
  TEST_ASSERT_EQUAL_INT(9, t.as_int(9)); // wrong type -> fallback
  TEST_ASSERT_EQUAL_STRING("hello", t.as_text().c_str());
  TEST_ASSERT_EQUAL_STRING("x", i.as_text("x").c_str());
  TEST_ASSERT_EQUAL_INT((int)ui::Color::White, (int)c.as_color());
  TEST_ASSERT_EQUAL_INT((int)ui::Align::Center, (int)a.as_align());
  TEST_ASSERT_EQUAL_INT((int)ui::HAlign::Right, (int)h.as_halign());
  TEST_ASSERT_EQUAL_INT((int)ui::VAlign::Bottom, (int)v.as_valign());
  TEST_ASSERT_NULL(f.as_font());

  TEST_ASSERT_EQUAL_STRING("int(5)", i.dump().c_str());
  TEST_ASSERT_EQUAL_STRING("text(\"hello\")", t.dump().c_str());
  TEST_ASSERT_EQUAL_STRING("bool(true)", b.dump().c_str());
  TEST_ASSERT_EQUAL_STRING("color(white)", c.dump().c_str());
  TEST_ASSERT_EQUAL_STRING("align(center)", a.dump().c_str());
}

void test_paramset_set_find_remove(void) {
  ui::ParamSet params{{"x", ui::Param(1)}, {"label", ui::Param("hi")}};
  TEST_ASSERT_EQUAL_size_t(2, params.size());
  TEST_ASSERT_TRUE(params.has("x"));
  TEST_ASSERT_EQUAL_INT(1, params.get_int("x"));
  TEST_ASSERT_EQUAL_INT(7, params.get_int("missing", 7));

  params.set("x", ui::Param(3)); // overwrite, not append
  TEST_ASSERT_EQUAL_size_t(2, params.size());
  TEST_ASSERT_EQUAL_INT(3, params.get_int("x"));

  params.set("y", ui::Param(4));
  TEST_ASSERT_EQUAL_size_t(3, params.size());
  TEST_ASSERT_EQUAL_INT(4, params.get_int("y"));

  params.remove("y");
  TEST_ASSERT_FALSE(params.has("y"));
  TEST_ASSERT_EQUAL_size_t(2, params.size());

  TEST_ASSERT_EQUAL_STRING("{x=int(3), label=text(\"hi\")}",
                           params.dump().c_str());
}

void test_validation_reports_unknown_and_range(void) {
  const std::vector<ui::ParamSpec> specs = {
      {"x", ui::ParamType::Int, true, -10, 10},
      {"text", ui::ParamType::Text, false},
  };

  const ui::ParamSet params{{"x", ui::Param(50)}, {"bogus", ui::Param(1)}};
  const std::vector<ui::ParamError> errors = ui::validate_params(params, specs);

  TEST_ASSERT_EQUAL_size_t(2, errors.size());
  TEST_ASSERT_EQUAL_STRING("x", errors[0].key.c_str());
  TEST_ASSERT_EQUAL_STRING("out of range: 50 not in [-10, 10]",
                           errors[0].message.c_str());
  TEST_ASSERT_EQUAL_STRING("bogus", errors[1].key.c_str());
  TEST_ASSERT_EQUAL_STRING("unknown parameter", errors[1].message.c_str());
}

void test_validation_reports_wrong_type(void) {
  const std::vector<ui::ParamSpec> specs = {
      {"x", ui::ParamType::Int, true, -10, 10},
  };
  const ui::ParamSet params{{"x", ui::Param("nope")}};
  const std::vector<ui::ParamError> errors = ui::validate_params(params, specs);
  TEST_ASSERT_EQUAL_size_t(1, errors.size());
  TEST_ASSERT_EQUAL_STRING("expected int, got text", errors[0].message.c_str());
}

void test_validation_reports_missing_required(void) {
  const std::vector<ui::ParamSpec> specs = {
      {"x", ui::ParamType::Int, true, -10, 10},
      {"text", ui::ParamType::Text, true},
  };
  const std::vector<ui::ParamError> errors =
      ui::validate_params(ui::ParamSet{}, specs);
  TEST_ASSERT_EQUAL_size_t(2, errors.size());
  TEST_ASSERT_EQUAL_STRING("x", errors[0].key.c_str());
  TEST_ASSERT_EQUAL_STRING("missing required parameter",
                           errors[0].message.c_str());
  TEST_ASSERT_EQUAL_STRING("text", errors[1].key.c_str());
}

// A required key that is present but empty is still missing: otherwise a label
// with Param() as its font validates cleanly and then draws nothing.
void test_validation_reports_empty_required(void) {
  const std::vector<ui::ParamSpec> specs = {
      {"font", ui::ParamType::Font, true},
  };
  const ui::ParamSet params{{"font", ui::Param()}};
  const std::vector<ui::ParamError> errors = ui::validate_params(params, specs);
  TEST_ASSERT_EQUAL_size_t(1, errors.size());
  TEST_ASSERT_EQUAL_STRING("font", errors[0].key.c_str());
  TEST_ASSERT_EQUAL_STRING("missing required parameter",
                           errors[0].message.c_str());

  // An empty optional value is simply absent, not an error.
  const std::vector<ui::ParamSpec> optional = {
      {"color", ui::ParamType::Color, false},
  };
  const ui::ParamSet with_empty{{"color", ui::Param()}};
  TEST_ASSERT_EQUAL_size_t(0,
                           ui::validate_params(with_empty, optional).size());
}

int main(int, char **) {
  UNITY_BEGIN();
  RUN_TEST(test_param_types_and_getters);
  RUN_TEST(test_paramset_set_find_remove);
  RUN_TEST(test_validation_reports_unknown_and_range);
  RUN_TEST(test_validation_reports_wrong_type);
  RUN_TEST(test_validation_reports_missing_required);
  RUN_TEST(test_validation_reports_empty_required);
  return UNITY_END();
}
