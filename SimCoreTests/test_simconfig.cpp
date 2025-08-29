#include <gtest/gtest.h>
#include "Core/Config/SimConfig.h"

using namespace simcore;

TEST(SimConfig, RoundTrip) {
    auto tmp = std::filesystem::temp_directory_path() / "soasim_cfg_test";
    std::filesystem::create_directories(tmp);
    auto cfgfile = tmp / "simulator.ini";

    SimConfig in{ tmp / "User", tmp / "QtBase" };
    std::string err;
    ASSERT_TRUE(SimConfigIO::Save(in, cfgfile, &err)) << err;

    auto out = SimConfigIO::Load(cfgfile, &err);
    ASSERT_TRUE(out.has_value()) << err;
    EXPECT_EQ(std::filesystem::weakly_canonical(in.user_dir),
        std::filesystem::weakly_canonical(out->user_dir));
    EXPECT_EQ(std::filesystem::weakly_canonical(in.qt_base_dir),
        std::filesystem::weakly_canonical(out->qt_base_dir));
}