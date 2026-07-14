#ifndef COMMON_BARYCENTRIC_HLSLI
#define COMMON_BARYCENTRIC_HLSLI


float2 clip_to_pixel(float4 clip_pos, float2 viewport_size)
{
	float2 ndc = clip_pos.xy / clip_pos.w;
	return float2(
        (ndc.x * 0.5f + 0.5f) * viewport_size.x,
        (0.5f - ndc.y * 0.5f) * viewport_size.y);
}

float edge_function(float2 p0, float2 p1, float2 p2)
{
	float2 e1 = p1 - p0;
	float2 e2 = p2 - p0;
	return e2.x * e1.y - e2.y * e1.x;
}

float3 calc_barycentric(float2 p, float2 p0, float2 p1, float2 p2)
{
	float area = edge_function(p0, p1, p2);
	float area_inv = rcp(area);
	return float3(
        edge_function(p1, p2, p) * area_inv,
        edge_function(p2, p0, p) * area_inv,
        edge_function(p0, p1, p) * area_inv);
}

struct BarycentricGradient
{
	float3 value;
	float3 dx;
	float3 dy;
};

BarycentricGradient calc_barycentric_with_grad(
    float2 p, float2 p0, float2 p1, float2 p2)
{
	float2 e1 = p1 - p0;
	float2 e2 = p2 - p0;
	float2 d = p - p0;
    
	float det = e1.x * e2.y - e1.y * e2.x;
	if (abs(det) < 1e-8f)
		det = 1e-8f;
	float det_inv = rcp(det);
    
	float lambda1 = (d.x * e2.y - d.y * e2.x) * det_inv;
	float lambda2 = (d.y * e1.x - d.x * e1.y) * det_inv;
	float lambda0 = 1.0f - lambda1 - lambda2;
    
	float2 grad_lambda1 = float2(e2.y, -e2.x) * det_inv;
	float2 grad_lambda2 = float2(-e1.y, e1.x) * det_inv;
	float2 grad_lambda0 = -grad_lambda1 - grad_lambda2;
    
	BarycentricGradient result;
	result.value = float3(lambda0, lambda1, lambda2);
	result.dx = float3(grad_lambda0.x, grad_lambda1.x, grad_lambda2.x);
	result.dy = float3(grad_lambda0.y, grad_lambda1.y, grad_lambda2.y);

	return result;
}

struct AttributeGrad
{
	float2 value;
	float2 dx;
	float2 dy;
};

AttributeGrad interpolate_uv_with_grad(
    float2 uv0, float2 uv1, float2 uv2,
    float3 q, float inv_D,
    float3 lambda, float3 d_lambda_dx, float3 d_lambda_dy)
{
	float2 N = lambda.x * uv0 * q.x + lambda.y * uv1 * q.y + lambda.z * uv2 * q.z;
	float2 uv = N * inv_D;
    
	float Dx = dot(d_lambda_dx, q);
	float Dy = dot(d_lambda_dy, q);
    
	float2 Nx = d_lambda_dx.x * uv0 * q.x + d_lambda_dx.y * uv1 * q.y + d_lambda_dx.z * uv2 * q.z;
	float2 Ny = d_lambda_dy.x * uv0 * q.x + d_lambda_dy.y * uv1 * q.y + d_lambda_dy.z * uv2 * q.z;
    
	AttributeGrad res;
	res.value = uv;
	res.dx = (Nx - uv * Dx) * inv_D;
	res.dy = (Ny - uv * Dy) * inv_D;
    
	return res;
}

#endif  // COMMON_BARYCENTRIC_HLSLI