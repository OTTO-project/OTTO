#include "testing.t.hpp"

#include <functional>

#include "core/props/props.hpp"

namespace otto::core::props {

  // Make sure that tags are sorted and uniquified correctly

  BOOST_HANA_CONSTANT_CHECK(
    black_magic::sort_tags(tag_list<mixins::has_value, mixins::steppable>()) ==
    black_magic::sort_tags(tag_list<mixins::steppable, mixins::has_value>()));

  BOOST_HANA_CONSTANT_CHECK(
    black_magic::sort_tags(
      tag_list<mixins::has_value, mixins::steppable, mixins::has_value>()) ==
    black_magic::sort_tags(tag_list<mixins::steppable, mixins::has_value>()));

  // Make sure that requirements are pulled in, and still sorted correctly

  BOOST_HANA_CONSTANT_CHECK(
    black_magic::with_required(tag_list<mixins::steppable>()) ==
    black_magic::sort_tags(tag_list<mixins::steppable, mixins::has_value>()));

  // Test the public `make_tag_list_t` type

  BOOST_HANA_CONSTANT_CHECK(
    make_tag_list_t<mixins::steppable>() ==
    black_magic::sort_tags(tag_list<mixins::steppable, mixins::has_value>()));

  TEST_CASE("Propeties", "[props]")
  {
    SECTION("Basic mixin tests")
    {
      // Test the tags of a property

      Property<float, mixins::steppable, mixins::has_name> pf;

      BOOST_HANA_CONSTANT_CHECK(
        pf.tag_list ==
        black_magic::sort_tags(
          tag_list<mixins::steppable, mixins::has_value, mixins::has_name>()));

      // Assert that the mixin implementation is inherited by the property
      static_assert(
        std::is_base_of_v<MixinTag::mixin_t<mixins::steppable, float,
                                            make_tag_list_t<mixins::steppable,
                                                            mixins::has_name>>,
                          decltype(pf)>);

      pf.set(3.f);
      REQUIRE(pf.get() == 3.f);
      pf.init<mixins::steppable>(2.f);
      pf.init<mixins::has_name>("property");
      pf.step(1);
      REQUIRE(pf == 5.f);
      float f = pf;
    }

    struct Props {
      Property<float,
               mixins::steppable,
               mixins::has_limits,
               mixins::has_name,
               mixins::faust_link>
        pf1 = 0.f;
      Property<float, mixins::steppable, mixins::has_name, mixins::faust_link>
        pf2 = 1.f;

      Props()
      {
        pf1.init<mixins::steppable>(1.f).init<mixins::has_limits>(-5.f, 5.f);
      };
    } props;

    CONCEPT_ASSERT(cpts::models<HookTag, mixins::has_value::hooks::on_set,
                                typename decltype(props.pf1)::value_type>());

    static_assert(MixinImpl::has_handler_v<
                  typename decltype(props.pf1)::mixin<mixins::has_limits>,
      mixins::has_value::hooks::on_set, HookOrder::Early>);

    REQUIRE(props.pf1 == 0.f);
    REQUIRE(props.pf2 == 1.f);
    props.pf1 = 10.f;
    // Test limits
    REQUIRE(props.pf1 == 5.f);
  }
} // namespace otto::core::props

#if false
namespace otto::core::props {

  Property<float, steppable, has_limits, serializable> property = {
    steppable::init(0),
    has_limits::init(0, 10),
    serializable::init()
  };

  struct Props : Properties {
    Property<float, has_name, has_parent> prop1 = {
      0,
      has_name::init("prop1"),
      has_parent::init(this)
    };
    Property<float, has_name, has_parent> prop2 = {
      0,
      has_name::init("prop2"),
      has_parent::init(this)
    };
  } props;

  TEST_CASE("Property conversions", "") {
    auto& pb = static_cast<PropertyBase&>(property);
    REQUIRE(pb == property);

    const float& val = static_cast<const float&>(property);
    REQUIRE(&val == &property.value());
  }

  TEST_CASE("Property children", "") {
    REQUIRE(props.size() == 2);
    REQUIRE(props[0] == props.prop1);
    REQUIRE(props[1] == props.prop2)
  }
}
#endif