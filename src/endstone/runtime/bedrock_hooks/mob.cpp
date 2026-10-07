// Copyright (c) 2024, The Endstone Project. (https://endstone.dev) All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "bedrock/world/actor/mob.h"

#include <iostream>

#include <cmath>
#include <random>

#include "bedrock/world/attribute/attribute_instance.h"
#include "bedrock/entity/components/damage_sensor_component.h"
#include "bedrock/entity/components/no_action_time_component.h"
#include "endstone/actor/actor.h"
#include "endstone/actor/mob.h"
#include "endstone/core/actor/mob.h"
#include "endstone/core/damage/damage_source.h"
#include "endstone/core/entity/components/flag_components.h"
#include "endstone/core/server.h"
#include "endstone/event/actor/actor_damage_event.h"
#include "endstone/event/actor/actor_knockback_event.h"
#include "endstone/runtime/hook.h"

void Mob::knockback(Actor *source, float damage, float dx, float dz, const KnockbackParameters &parameters)
{
    // If dx and dz are zero and a source is provided, compute the directional vector
    if (dx == 0.0f && dz == 0.0f && source != nullptr) {
        dx = getPosition().x - source->getPosition().x;
        dz = getPosition().z - source->getPosition().z;
    }

    float f = std::sqrt(dx * dx + dz * dz);
    if (f <= 0.0f) {
        return;
    }

    // Check Knockback Resistance (as in PocketMine-MP / axolotl-pm Living::knockBack)
    float knockback_resistance = 0.0f;
    try {
        if (const auto *attr = getAttribute("minecraft:knockback_resistance")) {
            knockback_resistance = attr->getCurrentValue();
        }
    }
    catch (...) {
        knockback_resistance = 0.0f;
    }

    // mt_rand() / mt_getrandmax() > knockbackResistanceAttr->getValue()
    static thread_local std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    if (knockback_resistance > 0.0f && dis(gen) <= knockback_resistance) {
        return;
    }

    const auto before = getPosDelta();

    // Default values from axolotl-pm / PocketMine-MP:
    // Living::DEFAULT_KNOCKBACK_FORCE = 0.4
    // Living::DEFAULT_KNOCKBACK_VERTICAL_LIMIT = 0.4
    constexpr float default_knockback_force = 0.4f;
    constexpr float default_vertical_limit = 0.4f;

    float force = default_knockback_force;
    float vertical_limit = default_vertical_limit;

    // In axolotl-pm, knockback enchantment adds level * 0.5f to force.
    // In Bedrock, parameters.extra_knockback_power carries enchantment / sprint bonus.
    if (parameters.extra_knockback_power > 0.0f) {
        force += parameters.extra_knockback_power * 0.5f;
    }

    float inv_f = 1.0f / f;
    float motion_x = (before.x / 2.0f) + (dx * inv_f * force);
    float motion_y = (before.y / 2.0f) + force;
    float motion_z = (before.z / 2.0f) + (dz * inv_f * force);

    if (motion_y > vertical_limit) {
        motion_y = vertical_limit;
    }

    Vec3 new_motion{motion_x, motion_y, motion_z};
    Vec3 diff = new_motion - before;

    const auto &server = endstone::core::EndstoneServer::getInstance();
    endstone::ActorKnockbackEvent e{getEndstoneActor<endstone::core::EndstoneMob>(),
                                    source == nullptr ? nullptr : &source->getEndstoneActor(),
                                    {diff.x, diff.y, diff.z}};
    server.getPluginManager().callEvent(e);

    const auto knockback = e.getKnockback();
    diff = e.isCancelled() ? Vec3::ZERO : Vec3{knockback.getX(), knockback.getY(), knockback.getZ()};
    setPosDelta(before + diff);
}

// bool Mob::_hurt(const ActorDamageSource &source, float damage, bool knock, bool ignite)
// {
//     addOrRemoveComponent<endstone::core::MobHurtFlagComponent>(true);
//     auto result = ENDSTONE_HOOK_CALL_ORIGINAL(&Mob::_hurt, this, source, damage, knock, ignite);
//     if (!hasComponent<endstone::core::MobHurtFlagComponent>()) {
//         // A related ActorDamageEvent is triggered and cancelled, propagate the result to the caller to prevent kb
//         // See also: HealthAttributeDelegate::change
//         return false;
//     }
//     addOrRemoveComponent<endstone::core::MobHurtFlagComponent>(false);
//     return result;
// }
