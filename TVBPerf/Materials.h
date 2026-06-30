#pragma once

#include <cstdint>
#include <cstddef>

class Materials
{
private:
	uint32_t m_material_count;
	uint32_t m_material_attribute_float4_count;
public:
	uint32_t get_material_count() const { return m_material_count; }
	uint32_t get_material_attribute_float4_count() const { return m_material_attribute_float4_count; }

	void init(uint32_t material_count, uint32_t material_attribute_float4_count);
};

