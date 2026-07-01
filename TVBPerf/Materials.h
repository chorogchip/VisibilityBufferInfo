#pragma once

#include <cstdint>
#include <cstddef>

class Materials
{
private:
	uint32_t material_count_;
	uint32_t material_attribute_float4_count_;
public:
	Materials() = default;

	uint32_t get_material_count() const { return material_count_; }
	uint32_t get_material_attribute_float4_count() const { return material_attribute_float4_count_; }

	void init(uint32_t material_count, uint32_t material_attribute_float4_count);
};

