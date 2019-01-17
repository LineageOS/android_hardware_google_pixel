#ifndef HARDWARE_GOOGLE_PIXEL_POWERSTATS_WLANSTATERESIDENCYDATAPROVIDER_H
#define HARDWARE_GOOGLE_PIXEL_POWERSTATS_WLANSTATERESIDENCYDATAPROVIDER_H

#include <pixelpowerstats/PowerStats.h>

namespace android {
namespace hardware {
namespace google {
namespace pixel {
namespace powerstats {

class WlanStateResidencyDataProvider : public IStateResidencyDataProvider {
  public:
    WlanStateResidencyDataProvider(uint32_t id, std::string path);
    ~WlanStateResidencyDataProvider() = default;
    bool getResults(std::map<uint32_t, PowerEntityStateResidencyResult> &results) override;
    std::vector<PowerEntityStateSpace> getStateSpaces() override;

  private:
    const std::string mPath;
    const uint32_t mPowerEntityId;
};

}  // namespace powerstats
}  // namespace pixel
}  // namespace google
}  // namespace hardware
}  // namespace android

#endif  // HARDWARE_GOOGLE_PIXEL_POWERSTATS_WLANSTATERESIDENCYDATAPROVIDER_H