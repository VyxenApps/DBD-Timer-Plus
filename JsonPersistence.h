#pragma once

#include "AppConfig.h"

AppConfig loadConfig();
void saveConfig(const AppConfig& config);
void createDefaults();
bool configExists();
void applySave(const AppConfig& config);
void applyLive(const AppConfig& config);
