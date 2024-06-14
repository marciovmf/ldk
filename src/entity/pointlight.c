#include <ldk/entity/pointlight.h>
#include <ldk/asset/mesh.h>
#include <ldk/maths.h>
#include <ldk/os.h>
#include <memory.h>

LDKPointLight* ldkPointLightEntityCreate(LDKPointLight* entity)
{
  entity->position = vec3Zero();
  entity->range = 0.3f;
  entity->colorDiffuse = vec3One();
  entity->colorSpecular = vec3One();
  entity->linear = 0.7f;
  entity->quadratic = 1.8f;
  return entity;
}

void ldkPointLightEntityDestroy(LDKPointLight* entity)
{
}

void ldkPointLightSetAttenuationPreset(LDKPointLight* entity, float range)
{
  entity->range = range;
  uint32 r = (uint32) range;

  // Attenuation values from https://wiki.ogre3d.org/tiki-index.php?page=-Point+Light+Attenuation
  uint32 rangeLimit[] = {7, 13, 20, 32, 50, 65, 100, 160, 200, 325, 600, 3250};
  double linear[] = {0.7, 0.35, 0.22, 0.14, 0.09, 0.07, 0.045, 0.027, 0.022, 0.014, 0.007, 0.0014};
  double quadratic[] = {1.8, 0.44, 0.20, 0.07, 0.032, 0.017, 0.0075, 0.0028, 0.0019, 0.0007, 0.0002, 0.000007};

  uint32 presetIndex = 0;
  uint32 numPresets = sizeof(rangeLimit) / sizeof(uint32);
  for (uint32 i = 0; i < numPresets; i++)
  {
    if (r > rangeLimit[i])
      break;
    presetIndex = i;
  }

  entity->linear    = (float) linear[presetIndex];
  entity->quadratic = (float) quadratic[presetIndex];
}

void ldkPointLightSetAttenuation(LDKPointLight* entity, float range, float linear, float quadratic)
{
  entity->range = range;
  entity->linear = linear;
  entity->quadratic = quadratic;
}
