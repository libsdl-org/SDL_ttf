#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "testgputext/spir-v.h"
#define SDL_MATH_3D_IMPLEMENTATION
#include "SDL_math3d.h"


#define MAX_VERTEX_COUNT 4000
#define MAX_INDEX_COUNT  6000


typedef struct Vec2 {
	float x, y;
} Vec2;

typedef struct Vec3 {
	float x, y, z;
} Vec3;


typedef struct Vertex {
	Vec3 pos;
	SDL_FColor colour;
	Vec2 uv;
} Vertex;


typedef struct Context {
	SDL_GPUDevice *device;
	SDL_Window *window;
	SDL_GPUGraphicsPipeline *pipeline;
	SDL_GPUBuffer *vertex_buffer;
	SDL_GPUBuffer *index_buffer;
	SDL_GPUTransferBuffer *transfer_buffer;
	SDL_GPUSampler *sampler;
	SDL_GPUCommandBuffer *cmd_buf;
} Context;


typedef struct GeometryData {
	Vertex *vertices;
	int vertex_count;
	int *indices;
	int index_count;
} GeometryData;


void check_error_bool(const bool res) {
	if (!res)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
}


void* check_error_ptr(void *ptr) {
	if (!ptr)
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());

	return ptr;
}


SDL_GPUShader* load_shader(
	SDL_GPUDevice* device,
	const unsigned char code[],
	const unsigned int code_size,
	bool is_vertex,
	Uint32 sampler_count,
	Uint32 uniform_buffer_count,
	Uint32 storage_buffer_count,
	Uint32 storage_texture_count
) {
	SDL_GPUShaderCreateInfo shader_info = {
		.code = code,
		.code_size = code_size,
		.entrypoint = "main",
		.format = SDL_GPU_SHADERFORMAT_SPIRV,
		.stage = (is_vertex)? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT,
		.num_samplers = sampler_count,
		.num_uniform_buffers = uniform_buffer_count,
		.num_storage_buffers = storage_buffer_count,
		.num_storage_textures = storage_texture_count
	};
	SDL_GPUShader* shader = SDL_CreateGPUShader(device, &shader_info);
	if (shader == NULL)
	{
		SDL_Log("Failed to create shader!");
		return NULL;
	}

	return shader;
}


void queue_text_sequence(GeometryData *geometry_data, TTF_GPUAtlasDrawSequence *sequence, SDL_FColor *colour) {
	for (int i = 0; i < sequence->num_vertices; i++) {
		Vertex vert;
		const float *xy = (float *)((Uint8 *)sequence->xy + i*sequence->xy_stride);
		vert.pos = (Vec3) {xy[0], xy[1], 0.0f};

		vert.colour = *colour;

		const float *uv = (float *)((Uint8 *)sequence->uv + i*sequence->uv_stride);
		SDL_memcpy(&vert.uv, uv, 2*sizeof(float));

		geometry_data->vertices[geometry_data->vertex_count + i] = vert;
	}

	SDL_memcpy(geometry_data->indices + geometry_data->index_count, sequence->indices, sequence->num_indices*sizeof(int));

	geometry_data->vertex_count += sequence->num_vertices;
	geometry_data->index_count += sequence->num_indices;
}

void queue_text(GeometryData *geometry_data, TTF_GPUAtlasDrawSequence *sequence, SDL_FColor *colour) {
	do {
		queue_text_sequence(geometry_data, sequence, colour);
	} while ((sequence = sequence->next));
}

void set_geometry_data(Context *context, GeometryData *geometry_data) {
	Vertex *transfer_data = SDL_MapGPUTransferBuffer(context->device, context->transfer_buffer, false);

	SDL_memcpy(transfer_data, geometry_data->vertices, sizeof(Vertex)*geometry_data->vertex_count);
	SDL_memcpy(transfer_data + MAX_VERTEX_COUNT, geometry_data->indices, sizeof(int)*geometry_data->index_count);

	SDL_UnmapGPUTransferBuffer(context->device, context->transfer_buffer);
}


void transfer_data(Context *context, GeometryData *geometry_data) {
	SDL_GPUCopyPass *copy_pass = check_error_ptr(SDL_BeginGPUCopyPass(context->cmd_buf));
	SDL_UploadToGPUBuffer(
		copy_pass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = context->transfer_buffer,
			.offset = 0
		},
		&(SDL_GPUBufferRegion) {
			.buffer = context->vertex_buffer,
			.offset = 0,
			.size = sizeof(Vertex) * geometry_data->vertex_count
		},
		false
	);
	SDL_UploadToGPUBuffer(
		copy_pass,
		&(SDL_GPUTransferBufferLocation) {
			.transfer_buffer = context->transfer_buffer,
			.offset = sizeof(Vertex) * MAX_VERTEX_COUNT
		},
		&(SDL_GPUBufferRegion) {
			.buffer = context->index_buffer,
			.offset = 0,
			.size = sizeof(int) * geometry_data->index_count
		},
		false
	);
	SDL_EndGPUCopyPass(copy_pass);
}


void draw(Context *context, SDL_Mat4X4 *matrices, int num_matrices, TTF_GPUAtlasDrawSequence *draw_sequence) {
    SDL_GPUTexture* swapchain_texture;
    check_error_bool(SDL_AcquireGPUSwapchainTexture(context->cmd_buf, context->window, &swapchain_texture, NULL, NULL));

	if (swapchain_texture != NULL)
	{
		SDL_GPUColorTargetInfo colour_target_info = { 0 };
		colour_target_info.texture = swapchain_texture;
		colour_target_info.clear_color = (SDL_FColor){ 0.3f, 0.4f, 0.5f, 1.0f };
		colour_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
		colour_target_info.store_op = SDL_GPU_STOREOP_STORE;

		SDL_GPURenderPass* render_pass = SDL_BeginGPURenderPass(context->cmd_buf, &colour_target_info, 1, NULL);

		SDL_BindGPUGraphicsPipeline(render_pass, context->pipeline);
		SDL_BindGPUVertexBuffers(
			render_pass, 0,
			&(SDL_GPUBufferBinding) {
				.buffer = context->vertex_buffer, .offset = 0
			},
			1
		);
		SDL_BindGPUIndexBuffer(
			render_pass,
			&(SDL_GPUBufferBinding) {
				.buffer = context->index_buffer, .offset = 0
			},
			SDL_GPU_INDEXELEMENTSIZE_32BIT
		);
		SDL_PushGPUVertexUniformData(context->cmd_buf, 0, matrices, sizeof(SDL_Mat4X4)*num_matrices);

		int index_offset = 0;
		for (TTF_GPUAtlasDrawSequence *seq = draw_sequence; seq != NULL; seq = seq->next) {
			SDL_BindGPUFragmentSamplers(
				render_pass, 0,
				&(SDL_GPUTextureSamplerBinding) {
					.texture = seq->atlas_texture, .sampler = context->sampler
				},
				1
			);

			SDL_DrawGPUIndexedPrimitives(render_pass, seq->num_indices, 1, index_offset, 0, 0);

			index_offset += seq->num_indices;
		}
		SDL_EndGPURenderPass(render_pass);
	}
}


void free_context(Context *context) {
	SDL_ReleaseGPUTransferBuffer(context->device, context->transfer_buffer);
	SDL_ReleaseGPUSampler(context->device, context->sampler);
	SDL_ReleaseGPUBuffer(context->device, context->vertex_buffer);
	SDL_ReleaseGPUBuffer(context->device, context->index_buffer);
	SDL_ReleaseGPUGraphicsPipeline(context->device, context->pipeline);
	SDL_DestroyGPUDevice(context->device);
	SDL_DestroyWindow(context->window);
}


int main(int argc, char *argv[]) {
	(void)argc; (void)argv;

	check_error_bool(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS));

	bool running = true;
	Context context = {0};

	context.window = check_error_ptr(SDL_CreateWindow("GPU text test", 800, 600, 0));

	context.device = check_error_ptr(SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, NULL));
	check_error_bool(SDL_ClaimWindowForGPUDevice(context.device, context.window));

	SDL_GPUShader *vertex_shader = load_shader(context.device, shader_vert_spv, shader_vert_spv_len, true, 0, 1, 0, 0);
	SDL_GPUShader *fragment_shader = load_shader(context.device, shader_frag_spv, shader_frag_spv_len, false, 1, 0, 0, 0);

	SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {
		.target_info = {
			.num_color_targets = 1,
			.color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
				.format = SDL_GetGPUSwapchainTextureFormat(context.device, context.window),
				.blend_state = (SDL_GPUColorTargetBlendState){
					.enable_blend = true,
					.alpha_blend_op = SDL_GPU_BLENDOP_ADD,
					.color_blend_op = SDL_GPU_BLENDOP_ADD,
					.color_write_mask = 0xF,
					.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
					.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA,
					.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
					.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA
				}
			}},
		},
		.vertex_input_state = (SDL_GPUVertexInputState){
			.num_vertex_buffers = 1,
			.vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){{
				.slot = 0,
				.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
				.instance_step_rate = 0,
				.pitch = sizeof(Vertex)
			}},
			.num_vertex_attributes = 3,
			.vertex_attributes = (SDL_GPUVertexAttribute[]){{
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
				.location = 0,
				.offset = 0
			}, {
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
				.location = 1,
				.offset = sizeof(float) * 3
			}, {
				.buffer_slot = 0,
				.format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
				.location = 2,
				.offset = sizeof(float) * 7
			}}
		},
		.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
		.vertex_shader = vertex_shader,
		.fragment_shader = fragment_shader
	};
	context.pipeline = check_error_ptr(SDL_CreateGPUGraphicsPipeline(context.device, &pipeline_create_info));

	SDL_ReleaseGPUShader(context.device, vertex_shader);
	SDL_ReleaseGPUShader(context.device, fragment_shader);

	check_error_bool(TTF_Init());
	TTF_Font *font = check_error_ptr(TTF_OpenFont("/usr/share/fonts/truetype/noto/NotoSansMono-Regular.ttf", 20));
	TTF_TextEngine *engine = check_error_ptr(TTF_CreateGPUTextEngine(context.device));
	TTF_Text *text = check_error_ptr(TTF_CreateText_Wrapped(engine, font, "hello 1234\nSDL is cool!", 0, 0));
	text->color = (SDL_FColor) {1.0f, 1.0f, 0.0f, 1.0f};

	SDL_GPUBufferCreateInfo vbf_info = {
		.usage = SDL_GPU_BUFFERUSAGE_VERTEX,
		.size = sizeof(Vertex) * MAX_VERTEX_COUNT
	};
	context.vertex_buffer = check_error_ptr(SDL_CreateGPUBuffer(context.device, &vbf_info));

	SDL_GPUBufferCreateInfo ibf_info = {
		.usage = SDL_GPU_BUFFERUSAGE_INDEX,
		.size = sizeof(int) * MAX_INDEX_COUNT
	};
	context.index_buffer = check_error_ptr(SDL_CreateGPUBuffer(context.device, &ibf_info));

	SDL_GPUTransferBufferCreateInfo tbf_info = {
		.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
		.size = (sizeof(Vertex) * MAX_VERTEX_COUNT) + (sizeof(int) * MAX_INDEX_COUNT)
	};
	context.transfer_buffer = check_error_ptr(SDL_CreateGPUTransferBuffer(context.device, &tbf_info));

	SDL_GPUSamplerCreateInfo sampler_info = {
		.min_filter = SDL_GPU_FILTER_NEAREST,
		.mag_filter = SDL_GPU_FILTER_NEAREST,
		.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
		.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
		.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE
	};
	context.sampler = check_error_ptr(SDL_CreateGPUSampler(context.device, &sampler_info));

	GeometryData geometry_data = {0};
	geometry_data.vertices = SDL_calloc(MAX_VERTEX_COUNT, sizeof(Vertex));
	geometry_data.indices = SDL_calloc(MAX_INDEX_COUNT, sizeof(int));

	SDL_Mat4X4 model = SDL_MatrixIdentity();
	model = SDL_MatrixMultiply(model, SDL_MatrixTranslation((SDL_Vec3) {300.0f, 150.0f, 0.0f}));
	model = SDL_MatrixMultiply(model, SDL_MatrixRotationZ(SDL_PI_F/4.0f));
	model = SDL_MatrixMultiply(model, SDL_MatrixScaling((SDL_Vec3) {1.2f, 1.2f, 1.0f}));

	SDL_Mat4X4 *matrices = (SDL_Mat4X4[]) {
		SDL_MatrixOrtho(0.0f, 800.0f, 600.0f, 0.0f, 0.1f, 100.0f),
		model
	};

	while (running) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
				case SDL_EVENT_QUIT:
					running = false;
					break;
			}
		}

		TTF_GPUAtlasDrawSequence *sequence = TTF_GetGPUTextDrawData(text);
		queue_text(&geometry_data, sequence, &text->color);

		set_geometry_data(&context, &geometry_data);

		context.cmd_buf = check_error_ptr(SDL_AcquireGPUCommandBuffer(context.device));
		transfer_data(&context, &geometry_data);
		draw(&context, matrices, 2, sequence);
		SDL_SubmitGPUCommandBuffer(context.cmd_buf);

		geometry_data.vertex_count = 0;
		geometry_data.index_count = 0;
	}

	SDL_free(geometry_data.vertices);
	SDL_free(geometry_data.indices);
	TTF_DestroyGPUTextEngine(engine);
	free_context(&context);
	SDL_Quit();

	return 0;
}
