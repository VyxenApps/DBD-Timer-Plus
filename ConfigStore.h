#pragma once
#include <functional>
#include <vector>
#include <string>
#include <fstream>
#include "AppConfig.h"
#include "JsonPersistence.h"

class ConfigStore
{
public:
	using Listener = std::function<void(const AppConfig&)>;

private:
	AppConfig data_;
	std::vector<Listener> listeners_;
	std::wstring filePath_;

public:
	ConfigStore() = default;

	void load(const std::wstring& path)
	{
		filePath_ = path;
		data_ = loadConfig();
	}

	void save()
	{
		if (!filePath_.empty())
			saveConfig(data_);
	}

	void set(const AppConfig& config)
	{
		data_ = config;
		notify();
	}

	void update(std::function<void(AppConfig&)> modifier)
	{
		modifier(data_);
		notify();
	}

	const AppConfig& get() const { return data_; }
	AppConfig& ref() { return data_; }

	void subscribe(Listener listener)
	{
		listeners_.push_back(std::move(listener));
	}

	void notify()
	{
		for (auto& listener : listeners_)
		{
			listener(data_);
		}
	}

	bool exists() const
	{
		if (filePath_.empty()) return false;
		std::wifstream probe(filePath_);
		return probe.is_open() && probe.good();
	}

	void createDefaults()
	{
		AppConfig defaults;
		data_ = defaults;
		save();
	}
};
