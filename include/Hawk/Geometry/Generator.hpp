#pragma once

#include <cinttypes>
#include <vector>
#include <type_traits>

#include <Hawk/Math/Functions.hpp>



namespace Hawk {
	namespace Geometry {

		template<typename T>
		[[nodiscard]] auto ComputeNormal(std::vector<Math::Vec3> const& position, std::vector<T> const& indices) -> std::vector<Math::Vec3> {
			static_assert(std::is_same<T, uint16_t>::value || std::is_same<T, uint32_t>::value, "Failed indices type");

			std::vector<Math::Vec3> normals;
			for (auto index = 0; index < indices.size(); index += 3) {

				auto v0 = position[indices[index + 0]];
				auto v1 = position[indices[index + 1]];
				auto v2 = position[indices[index + 2]];


				auto normal = Math::Cross(Math::Normalize(v1 - v0), Math::Normalize(v2 - v0));

				normals.push_back(normal);
				normals.push_back(normal);
				normals.push_back(normal);
			}

			return normals;
		}


		template<typename T>
		[[nodiscard]]  auto ComputeTangentSpace(std::vector<Math::Vec3> const& position, std::vector<Math::Vec3> const& normals, std::vector<Math::Vec2> const& texcoord, std::vector<T> const& indices) -> std::vector<Math::Vec4> {
			static_assert(std::is_same<T, uint16_t>::value || std::is_same<T, uint32_t>::value, "Failed indices type");

			std::vector<Math::Vec4> tangents;
			auto computeTangent = [](auto const& p0, auto const& p1, auto const& uv0, auto const& uv1, auto const& n) {
				auto det = 1.0f / (uv1.x * uv1.y - uv1.y * uv1.x);
				auto t = Math::Normalize(det * (p0 * uv1.y - p1 * uv0.y));
				auto b = Math::Normalize(det * (p1 * uv0.x - p1 * uv1.x));
				return Math::Vec4(Math::Normalize(t - n * Math::Dot(n, t)), Math::Dot(Math::Cross(n, t), b) < 0.0f ? -1.0f : 1.0f);
			};

			for (auto index = 0; index < indices.size(); index += 3) {
				auto v0 = position[indices[index + 0]];
				auto v1 = position[indices[index + 1]];
				auto v2 = position[indices[index + 2]];

				auto uv0 = texcoord[indices[index + 0]];
				auto uv1 = texcoord[indices[index + 1]];
				auto uv2 = texcoord[indices[index + 2]];

				auto t0 = computeTangent(v1 - v0, v2 - v0, uv1 - uv0, uv2 - uv0, normals[indices[index + 0]]);
				auto t1 = computeTangent(v2 - v1, v0 - v1, uv2 - uv1, uv0 - uv1, normals[indices[index + 1]]);
				auto t2 = computeTangent(v0 - v2, v1 - v2, uv0 - uv2, uv1 - uv2, normals[indices[index + 2]]);

				tangents.push_back(t0);
				tangents.push_back(t1);
				tangents.push_back(t2);
			}
			return tangents;
		}


		template<typename IndexType>
		class Mesh {
		public:
			std::vector<Math::Vec3> Vertices;
			std::vector<Math::Vec3> Normals;
			std::vector<Math::Vec4> Tangents;
			std::vector<Math::Vec2> Texcoords;
			std::vector<IndexType>  Indices;
		};

		class Generator {
		public:

			template<typename IndexType>
			[[nodiscard]] static auto GenerateTriangle(Math::Vec3 const& v0, Math::Vec3 const& v1, Math::Vec3 const& v2) -> Mesh<IndexType> {
				static_assert(std::is_same<IndexType, uint16_t>::value || std::is_same<IndexType, uint32_t>::value, "Undefined index type");

				Mesh<IndexType> mesh;
				mesh.Vertices.push_back(v0);
				mesh.Vertices.push_back(v1);
				mesh.Vertices.push_back(v2);

				mesh.Texcoords.emplace_back(0.0f, 0.0f);
				mesh.Texcoords.emplace_back(0.0f, 1.0f);
				mesh.Texcoords.emplace_back(1.0f, 1.0f);

				mesh.Indices.emplace_back(static_cast<IndexType>(0));
				mesh.Indices.emplace_back(static_cast<IndexType>(1));
				mesh.Indices.emplace_back(static_cast<IndexType>(2));

				mesh.Normals  = ComputeNormal(mesh.Vertices, mesh.Indices);
				mesh.Tangents = ComputeTangentSpace(mesh.Vertices, mesh.Normals, mesh.Texcoords, mesh.Indices);

			}

			template<typename IndexType>
			[[nodiscard]] static auto GenerateQuad(uint32_t teselation) -> Mesh<IndexType> {
				static_assert(std::is_same<IndexType, uint16_t>::value || std::is_same<IndexType, uint32_t>::value, "Undefined index type");

			
				return mesh;

			}

			template<typename IndexType>
			[[nodiscard]] static auto GenerateSphere(uint32_t teselation) -> Mesh<IndexType> {
				static_assert(std::is_same<IndexType, uint16_t>::value || std::is_same<IndexType, uint32_t>::value, "Undefined index type");

				if (tessellation < 3)
						throw std::out_of_range("Tesselation parameter out of range");
				
					Mesh<IndexType> model;
					auto verticalSegments = tessellation;
					auto horizontalSegments = tessellation * 2;
				
					for (auto i = 0; i <= verticalSegments; i++) {
						auto v = 1.0f - float(i) / verticalSegments;
						auto latitude = (i * 3.14159265f / verticalSegments) - 3.14159265f / 2.0f;
				
						auto dy = Math::Sin(latitude);
						auto dxz = Math::Cos(latitude);
				
						for (auto j = 0; j <= horizontalSegments; j++) {
							auto u = float(j) / horizontalSegments;
				
							auto longitude = j * 2.0f * 3.14159265f / horizontalSegments;
				
							auto dx = Math::Sin(longitude);
							auto dz = Math::Cos(longitude);
				
							dx *= dxz;
							dz *= dxz;
				
							auto normal = Math::Vec3(dx, dy, dz);
							auto uv = Math::Vec2(u, v);
				
							model.Vertices.push_back({ normal, normal, Math::Vec3(0.0f, 0.0f, 0.0f), uv });
						}
					}
				
					auto stride = horizontalSegments + 1;
				
					for (auto i = 0; i < verticalSegments; i++) {
						for (auto j = 0; j <= horizontalSegments; j++) {
				
							auto nextI = i + 1;
							auto nextJ = (j + 1) % stride;
				
							model.Indices.push_back(static_cast<IndexType>(i * stride + j));
							model.Indices.push_back(static_cast<IndexType>(nextI * stride + j));
							model.Indices.push_back(static_cast<IndexType>(i * stride + nextJ));
				
							model.Indices.push_back(static_cast<IndexType>(i * stride + nextJ));
							model.Indices.push_back(static_cast<IndexType>(nextI * stride + j));
							model.Indices.push_back(static_cast<IndexType>(nextI * stride + nextJ));
				
						}
					}
				
					for (auto index = 0; index < model.Indices.size(); index += 3) {
						auto const& v0 = model.Vertices[model.Indices[index + 0]];
						auto const& v1 = model.Vertices[model.Indices[index + 1]];
						auto const& v2 = model.Vertices[model.Indices[index + 2]];
				
						auto const& uv0 = model.Texcoord[model.Indices[index + 0]];
						auto const& uv1 = model.Texcoord[model.Indices[index + 1]];
						auto const& uv2 = model.Texcoord[model.Indices[index + 2]];
				
						auto deltaUV1 = uv1 - uv0;
						auto deltaUV2 = uv2 - uv0;
				
						auto deltaPos1 = v1 - v0;
						auto deltaPos2 = v2 - v0;
				
						auto f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
						auto t = Math::Normalize(f * (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y));
				
						model.Vertices[model.Indices[index + 0]].Tangent = t;
						model.Vertices[model.Indices[index + 1]].Tangent = t;
						model.Vertices[model.Indices[index + 2]].Tangent = t;
					}
					return model;

			}


		};



		

	};
}

