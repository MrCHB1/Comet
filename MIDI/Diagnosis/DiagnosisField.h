#pragma once
#include <vector>
#include <string>
#include <memory>
#include <format>
#include <mutex>
#include <optional>
#include "../../Utils.h"

class DiagnosisField
{
public:
	std::vector<std::shared_ptr<DiagnosisField>> additionalFields;
	virtual ~DiagnosisField() = default;
};

class KeyValue : public DiagnosisField
{
public:
	const std::string name;

	explicit KeyValue(std::string name) : name(std::move(name)) {}
	
	virtual std::string GetValue() = 0;
};

template <typename T>
class Number : public KeyValue
{
public:
	explicit Number(std::string name) : KeyValue(std::move(name)) {}

	std::string GetValue()
	{
		std::lock_guard<std::mutex> lock(mutex);

		if (cached.has_value()) return *cached;

		if (primary.has_value() && secondary.has_value())
		{
			std::string v1 = Format(*primary);
			std::string v2 = Format(*secondary);

			cached = (v1 == v2) ? v1 : (v1 + "~" + v2);
		}
		else if (primary.has_value())
		{
			cached = Format(*primary);
		}
		else if (secondary.has_value())
		{
			cached = Format(*secondary);
		}
		else
		{
			cached = " ";
		}
		return *cached;
	}

	void SetValue(T p)
	{
		SetValue(p, std::nullopt);
	}
	void SetValue(T p, std::optional<T> s)
	{
		std::lock_guard<std::mutex> lock(mutex);
		primary = p;
		secondary = s;
		cached.reset();
	}
	void SetPrimaryValue(T p)
	{
		std::lock_guard<std::mutex> lock(mutex);
		primary = p;
		cached.reset();
	}
	void SetSecondaryValue(T s)
	{
		std::lock_guard<std::mutex> lock(mutex);
		secondary = s;
		cached.reset();
	}
protected:
	virtual std::string Format(const T& value) = 0;
private:
	std::optional<T> primary;
	std::optional<T> secondary;
	std::optional<std::string> cached;
	std::mutex mutex;
};

class Long : public Number<long long>
{
public:
	explicit Long(std::string name) : Number<long long>(std::move(name)) {}
protected:
	std::string Format(const long long& val) override
	{
		return Utils::FormatWithCommas(val);
	}
};

class Double : public Number<double>
{
public:
	explicit Double(std::string name) : Number<double>(std::move(name)) {}
protected:
	std::string Format(const double& val) override
	{
		return std::to_string(val);
	}
};

class FileSize : public Long
{
public:
	explicit FileSize(std::string name) : Long(std::move(name)) {}
protected:
	std::string Format(const long long& val) override
	{
		return Utils::FormatFilesize(val, 2);
	}
};

class Duration : public Long
{
public:
	explicit Duration(std::string name) : Long(std::move(name)) {}
protected:
	std::string Format(const long long& val) override
	{
		return Utils::FormatDuration(val);
	}
};

class LongList : public DiagnosisField
{
private:
	std::shared_ptr<std::vector<long>> arr = std::make_shared<std::vector<long>>();
	std::mutex listMtx;
public:
	std::shared_ptr<std::vector<long>> GetValue()
	{
		std::lock_guard<std::mutex> lock(listMtx);
		return arr;
	}

	void SetValue(std::shared_ptr<std::vector<long>> arr)
	{
		std::lock_guard<std::mutex> lock(listMtx);
		this->arr = arr;
	}
};