#ifndef _TEMPERATUREALERT_H_
#define _TEMPERATUREALERT_H_

#include <SinricProDevice.h>
#include <Capabilities/ContactSensor.h>
#include <Capabilities/TemperatureSensor.h>

class TemperatureAlert 
: public SinricProDevice
, public ContactSensor<TemperatureAlert>
, public TemperatureSensor<TemperatureAlert> {
  friend class ContactSensor<TemperatureAlert>;
  friend class TemperatureSensor<TemperatureAlert>;
public:
  TemperatureAlert(const String &deviceId) : SinricProDevice(deviceId, "TemperatureAlert") {};
};

#endif
