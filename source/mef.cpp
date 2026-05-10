#include "mef.h"

#pragma pack(push, 1)

struct HSEM_chunk_s {
	uint32_t reserved0;
	uint32_t year, month, day, hour, min, sec, ms;
	uint32_t mt;
	uint32_t reserved1[3];
	float f[12];
	uint16_t fn;
	uint16_t vn;
	uint16_t reserved6;
	uint16_t cv;
	uint16_t cf;
	uint16_t reserved9;
	float fc;
	uint16_t mg;
	uint16_t an;
	uint16_t pv;
	uint16_t pf;
	uint16_t pn;
	uint16_t _f;
	uint8_t rs[20];
};

#pragma pack(pop)

bool MefLoader::LoadMEF(const char* filepath, mef_model_s& out_model) {
	int32_t file_len = 0;
	void* buf = nullptr;

	if (!File_LoadBinary(filepath, buf, file_len)) {
		return false;
	}

	uint8_t* ptr = (uint8_t*)buf;
	uint8_t* end = ptr + file_len;

	// Read ILFF
	if (file_len < 12) {
		File_FreeBuf(buf);
		return false;
	}

	if (memcmp(ptr, "ILFF", 4) != 0) {
		File_FreeBuf(buf);
		return false;
	}
	ptr += 8; // skip size
	if (memcmp(ptr, "OCEM", 4) != 0) {
		File_FreeBuf(buf);
		return false;
	}
	ptr += 4;

	while (ptr + 8 <= end) {
		char tag[5] = {0};
		memcpy(tag, ptr, 4);
		uint32_t chunk_size = *(uint32_t*)(ptr + 4);
		ptr += 8;

		if (ptr + chunk_size > end) break;
		uint8_t* chunk_data = ptr;
		ptr += chunk_size;

		if (strcmp(tag, "HSEM") == 0) {
			if (chunk_size >= sizeof(HSEM_chunk_s)) {
				HSEM_chunk_s* hsem = (HSEM_chunk_s*)chunk_data;
				out_model.type = hsem->mt;
			}
		} else if (strcmp(tag, "D3DR") == 0) {
			// Do nothing for now
		} else if (strcmp(tag, "DNER") == 0) {
			mef_submesh_s submesh;
			float* fptr = (float*)chunk_data;
			submesh.offset = glm::vec3(fptr[0], fptr[1], fptr[2]);

			int16_t* sptr = (int16_t*)(chunk_data + 12);
			int fn = sptr[0];

			uint16_t* indices;
			if (out_model.type == 3) { // Lightmap
				submesh.vertex_offset = sptr[4];
				submesh.vertex_count = sptr[5];
				indices = (uint16_t*)(chunk_data + 12 + 20); // 10 shorts = 20 bytes
			} else { // Type 0/1
				submesh.vertex_offset = sptr[3];
				submesh.vertex_count = sptr[4];
				indices = (uint16_t*)(chunk_data + 12 + 16); // 8 shorts = 16 bytes
			}

			for (int i = 0; i < fn / 3; i++) {
				mef_face_s face;
				face.v0 = indices[i * 3];
				face.v1 = indices[i * 3 + 1];
				face.v2 = indices[i * 3 + 2];
				submesh.faces.push_back(face);
			}

			out_model.submeshes.push_back(submesh);

		} else if (strcmp(tag, "XTRV") == 0) {
			int vertex_size = (out_model.type == 0) ? 32 : 40;
			int vertex_count = chunk_size / vertex_size;

			for (int i = 0; i < vertex_count; i++) {
				uint8_t* vptr = chunk_data + i * vertex_size;
				mef_vert_s vert;
				float* fptr = (float*)vptr;
				vert.pos = glm::vec3(fptr[0], fptr[1], fptr[2]);
				vert.normal = glm::vec3(fptr[3], fptr[4], fptr[5]);
				vert.uv0 = glm::vec2(fptr[6], fptr[7]);
				if (vertex_size == 40) {
					vert.uv1 = glm::vec2(fptr[8], fptr[9]);
				} else {
					vert.uv1 = glm::vec2(0.0f);
				}
				out_model.vertices.push_back(vert);
			}
		}
	}

	File_FreeBuf(buf);
	return true;
}
