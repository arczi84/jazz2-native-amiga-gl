#include "GLShaderProgram.h"

#if defined(WITH_AMIGA_OGLES2)
#	include <cstdio>
#	include <cstring>
#	include <string>
#endif

#include <cstdio>
#include <cstdlib>
#include "GLShader.h"
#include "GLDebug.h"
#include "../../BinaryShaderCache.h"
#include "../../RenderResources.h"
#include "../../RenderVaoPool.h"
#include "../IRhiCapabilities.h"
#include "../../../ServiceLocator.h"
#include "../../../Base/StaticHashMapIterator.h"
#include "../../../tracy.h"
#include "../../../../Shaders/Generated/ShaderCompilerTypes.h"

#include <cstring>
#include <string>

namespace nCine::RHI::GL
{
#if defined(WITH_AMIGA_OGLES2)
	namespace
	{
		/** @brief Text of a batched ES2 vertex shader's instance array: the "uniform <T> instances[BATCH_SIZE];" line and struct <T> */
		bool ParseInstanceArray(const std::string& src, std::string& typeName, std::size_t& lineStart, std::size_t& lineEnd,
			GLUniformBlock::FlatInstanceField* fields, std::uint32_t& fieldCount, std::uint32_t& stride)
		{
			const std::size_t decl = src.find(" instances[BATCH_SIZE];");
			if (decl == std::string::npos) {
				return false;
			}
			lineStart = src.rfind('\n', decl);
			lineStart = (lineStart == std::string::npos ? 0 : lineStart + 1);
			lineEnd = src.find('\n', decl);
			lineEnd = (lineEnd == std::string::npos ? src.size() : lineEnd + 1);

			// "<ws>uniform <T> instances[BATCH_SIZE];"
			std::size_t p = src.find("uniform ", lineStart);
			if (p == std::string::npos || p > decl) {
				return false;
			}
			p += 8;
			typeName = src.substr(p, decl - p);
			while (!typeName.empty() && (typeName.back() == ' ' || typeName.back() == '\t')) {
				typeName.pop_back();
			}

			const std::size_t structPos = src.find("struct " + typeName);
			if (structPos == std::string::npos) {
				return false;
			}
			const std::size_t open = src.find('{', structPos);
			const std::size_t close = src.find('}', open);
			if (open == std::string::npos || close == std::string::npos) {
				return false;
			}

			fieldCount = 0;
			std::uint32_t offset = 0, maxAlign = 16;
			std::size_t pos = open + 1;
			while (pos < close) {
				std::size_t end = src.find('\n', pos);
				if (end == std::string::npos || end > close) {
					end = close;
				}
				std::string line = src.substr(pos, end - pos);
				pos = end + 1;
				const std::size_t comment = line.find("//");
				if (comment != std::string::npos) {
					line.resize(comment);
				}
				char type[16], name[40];
				if (std::sscanf(line.c_str(), " %15s %39[A-Za-z0-9_] ;", type, name) != 2) {
					continue;
				}
				std::uint32_t floats, size, align;
				GLenum glType;
				if (std::strcmp(type, "mat4") == 0) { floats = 16; size = 64; align = 16; glType = GL_FLOAT_MAT4; }
				else if (std::strcmp(type, "vec4") == 0) { floats = 4; size = 16; align = 16; glType = GL_FLOAT_VEC4; }
				else if (std::strcmp(type, "vec3") == 0) { floats = 3; size = 12; align = 16; glType = GL_FLOAT_VEC3; }
				else if (std::strcmp(type, "vec2") == 0) { floats = 2; size = 8; align = 8; glType = GL_FLOAT_VEC2; }
				else if (std::strcmp(type, "float") == 0) { floats = 1; size = 4; align = 4; glType = GL_FLOAT; }
				else { return false; }
				if (fieldCount >= GLUniformBlock::MaxFlatInstanceFields || std::strlen(name) >= sizeof(fields[0].Name)) {
					return false;
				}
				offset = (offset + align - 1) / align * align;
				GLUniformBlock::FlatInstanceField& f = fields[fieldCount++];
				std::strcpy(f.Name, name);
				f.Location = -1;
				f.Type = glType;
				f.Offset = offset;
				f.Floats = floats;
				offset += size;
			}
			stride = (offset + maxAlign - 1) / maxAlign * maxAlign;
			return (fieldCount > 0);
		}

		/**
		 * @brief Splits the instance array of a batched ES2 vertex shader into one uniform array per field
		 *
		 * A uniform array of structs is one location per element and member in ES2, so uploading N instances
		 * would take 3-5 glUniform calls per sprite. One array per field takes one call per field for the
		 * whole batch; an accessor rebuilds the struct, so the shader body stays as it was.
		 */
		bool FlattenInstanceArray(const std::string& src, std::string& out)
		{
			static const char Access[] = "instances[int(aInstanceIndex)]";
			if (src.find(Access) == std::string::npos) {
				return false;
			}
			std::string typeName;
			std::size_t lineStart, lineEnd;
			GLUniformBlock::FlatInstanceField fields[GLUniformBlock::MaxFlatInstanceFields];
			std::uint32_t fieldCount, stride;
			if (!ParseInstanceArray(src, typeName, lineStart, lineEnd, fields, fieldCount, stride)) {
				return false;
			}
			static const char* TypeNames[] = { "mat4", "vec4", "vec3", "vec2", "float" };
			std::string decls;
			for (std::uint32_t i = 0; i < fieldCount; i++) {
				const char* typeText = (fields[i].Floats == 16 ? TypeNames[0] : fields[i].Floats == 4 ? TypeNames[1]
					: fields[i].Floats == 3 ? TypeNames[2] : fields[i].Floats == 2 ? TypeNames[3] : TypeNames[4]);
				decls += "uniform "; decls += typeText; decls += " instances_"; decls += fields[i].Name; decls += "[BATCH_SIZE];\n";
			}
			decls += typeName + " instances_get(int k)\n{\n\t" + typeName + " r;\n";
			for (std::uint32_t i = 0; i < fieldCount; i++) {
				decls += "\tr."; decls += fields[i].Name; decls += " = instances_"; decls += fields[i].Name; decls += "[k];\n";
			}
			decls += "\treturn r;\n}\n";

			out = src.substr(0, lineStart) + decls + src.substr(lineEnd);
			for (std::size_t p = out.find(Access); p != std::string::npos; p = out.find(Access, p)) {
				out.replace(p, sizeof(Access) - 1, "instances_get(int(aInstanceIndex))");
			}
			return true;
		}
	}
#endif

	namespace
	{
		GLenum UniformTypeToGL(ShaderCompiler::UniformType type)
		{
			switch (type) {
				case ShaderCompiler::UniformType::Float: return GL_FLOAT;
				case ShaderCompiler::UniformType::Int: return GL_INT;
				case ShaderCompiler::UniformType::UInt: return GL_UNSIGNED_INT;
#if !defined(DEATH_TARGET_VITA)
				case ShaderCompiler::UniformType::Bool: return GL_BOOL;
#endif
				case ShaderCompiler::UniformType::Vec2: return GL_FLOAT_VEC2;
				case ShaderCompiler::UniformType::Vec3: return GL_FLOAT_VEC3;
				case ShaderCompiler::UniformType::Vec4: return GL_FLOAT_VEC4;
				case ShaderCompiler::UniformType::IVec2: return GL_INT_VEC2;
				case ShaderCompiler::UniformType::IVec3: return GL_INT_VEC3;
				case ShaderCompiler::UniformType::IVec4: return GL_INT_VEC4;
#if !defined(DEATH_TARGET_VITA)	// vitaGL declares none of the bool / unsigned-int vector types
				case ShaderCompiler::UniformType::UVec2: return GL_UNSIGNED_INT_VEC2;
				case ShaderCompiler::UniformType::UVec3: return GL_UNSIGNED_INT_VEC3;
				case ShaderCompiler::UniformType::UVec4: return GL_UNSIGNED_INT_VEC4;
				case ShaderCompiler::UniformType::BVec2: return GL_BOOL_VEC2;
				case ShaderCompiler::UniformType::BVec3: return GL_BOOL_VEC3;
				case ShaderCompiler::UniformType::BVec4: return GL_BOOL_VEC4;
#endif
				case ShaderCompiler::UniformType::Mat2: return GL_FLOAT_MAT2;
				case ShaderCompiler::UniformType::Mat3: return GL_FLOAT_MAT3;
				case ShaderCompiler::UniformType::Mat4: return GL_FLOAT_MAT4;
				case ShaderCompiler::UniformType::Sampler2D: return GL_SAMPLER_2D;
#if !defined(DEATH_TARGET_VITA)	// GL_SAMPLER_3D is not declared by vitaGL
				case ShaderCompiler::UniformType::Sampler3D: return GL_SAMPLER_3D;
#endif
				case ShaderCompiler::UniformType::SamplerCube: return GL_SAMPLER_CUBE;
				default:
					LOGW("No available case to handle reflected type: {}", std::uint32_t(type));
					return GL_FLOAT;
			}
		}
	}

	GLuint GLShaderProgram::_boundProgram = 0;

	GLShaderProgram::GLShaderProgram()
		: GLShaderProgram(QueryPhase::Immediate)
	{
	}

	GLShaderProgram::GLShaderProgram(QueryPhase queryPhase)
		: _glHandle(0), _status(Status::NotLinked), _introspection(Introspection::Disabled), _queryPhase(queryPhase), _batchSize(DefaultBatchSize), _shouldLogOnErrors(true), _uniformsSize(0), _uniformBlocksSize(0), _reflection(nullptr)
	{
		_glHandle = glCreateProgram();

		_attachedShaders.reserve(AttachedShadersInitialSize);
		_uniforms.reserve(UniformsInitialSize);
		_uniformBlocks.reserve(UniformBlocksInitialSize);
		_attributes.reserve(AttributesInitialSize);
		
#if defined(RHI_GL_PROFILE_ES2)
		// GL_PROGRAM_BINARY_RETRIEVABLE_HINT (and glProgramParameteri) is ES 3.0; the ES2-era
		// OES_get_program_binary path retrieves binaries without a hint
#else
		if (RenderResources::GetBinaryShaderCache().IsAvailable()) {
			glProgramParameteri(_glHandle, GL_PROGRAM_BINARY_RETRIEVABLE_HINT, GL_TRUE);
		}
#endif
	}

	GLShaderProgram::GLShaderProgram(StringView vertexFile, StringView fragmentFile, Introspection introspection, QueryPhase queryPhase)
		: GLShaderProgram(queryPhase)
	{
		const bool hasCompiledVS = AttachShaderFromFile(ShaderStage::Vertex, vertexFile);
		const bool hasCompiledFS = AttachShaderFromFile(ShaderStage::Fragment, fragmentFile);

		if (hasCompiledVS && hasCompiledFS) {
			Link(introspection);
		}
	}

	GLShaderProgram::GLShaderProgram(StringView vertexFile, StringView fragmentFile, Introspection introspection)
		: GLShaderProgram(vertexFile, fragmentFile, introspection, QueryPhase::Immediate)
	{
	}

	GLShaderProgram::GLShaderProgram(StringView vertexFile, StringView fragmentFile)
		: GLShaderProgram(vertexFile, fragmentFile, Introspection::Enabled, QueryPhase::Immediate)
	{
	}

	GLShaderProgram::~GLShaderProgram()
	{
		if (_boundProgram == _glHandle) {
			glUseProgram(0);
		}

		glDeleteProgram(_glHandle);

#if !defined(WITH_RHI_SOFTWARE)
		// The render pipeline is built against a single backend's `RHI::` aliases; when the software
		// backend is selected these GL-typed pipeline calls do not apply (this file is dead code there)
		RenderResources::RemoveCameraUniformData(this);
#endif
	}

	bool GLShaderProgram::IsLinked() const
	{
		return (_status == Status::Linked ||
				_status == Status::LinkedWithDeferredQueries ||
				_status == Status::LinkedWithIntrospection);
	}

	std::uint32_t GLShaderProgram::RetrieveInfoLogLength() const
	{
		GLint length = 0;
		glGetProgramiv(_glHandle, GL_INFO_LOG_LENGTH, &length);
		return static_cast<std::uint32_t>(length);
	}

	void GLShaderProgram::RetrieveInfoLog(std::string& infoLog) const
	{
		GLint length = 0;
		glGetProgramiv(_glHandle, GL_INFO_LOG_LENGTH, &length);

		if (length > 0 && infoLog.capacity() > 0) {
			const std::uint32_t capacity = (std::uint32_t)infoLog.capacity();
			glGetProgramInfoLog(_glHandle, capacity, &length, infoLog.data());
			infoLog.resize(static_cast<std::uint32_t>(length) < capacity - 1 ? static_cast<std::uint32_t>(length) : capacity - 1);
		}
	}

	bool GLShaderProgram::AttachShaderFromFile(ShaderStage stage, StringView filename)
	{
		return AttachShaderFromStringsAndFile(stage, nullptr, filename);
	}

	bool GLShaderProgram::AttachShaderFromString(ShaderStage stage, StringView string)
	{
		return AttachShaderFromStringsAndFile(stage, arrayView({ string }), {});
	}

	bool GLShaderProgram::AttachShaderFromStrings(ShaderStage stage, ArrayView<const StringView> strings)
	{
		return AttachShaderFromStringsAndFile(stage, strings, {});
	}

	bool GLShaderProgram::AttachShaderFromStringsAndFile(ShaderStage stage, ArrayView<const StringView> strings, StringView filename)
	{
		std::unique_ptr<GLShader> shader = std::make_unique<GLShader>(stage == ShaderStage::Vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
#if defined(WITH_AMIGA_OGLES2)
		shader->IsVertex = (stage == ShaderStage::Vertex);
		// Batched ES2 programs: one uniform array per instance field instead of an array of structs
		std::string flattened;
		StringView flattenedParts[1];
		if (stage == ShaderStage::Vertex) {
			std::string joined;
			for (StringView part : strings) {
				joined.append(part.data(), part.size());
			}
			if (FlattenInstanceArray(joined, flattened)) {
				// A named array, not an initializer list: the view must outlive this block
				flattenedParts[0] = StringView(flattened.data(), flattened.size());
				strings = flattenedParts;
			}
		}
#endif
		shader->LoadFromStringsAndFile(strings, filename);
		glAttachShader(_glHandle, shader->GetGLHandle());

		const GLShader::ErrorChecking errorChecking = (_queryPhase == GLShaderProgram::QueryPhase::Immediate)
			? GLShader::ErrorChecking::Immediate
			: GLShader::ErrorChecking::Deferred;
		const bool hasCompiled = shader->Compile(errorChecking, _shouldLogOnErrors);

		if (hasCompiled) {
			_attachedShaders.push_back(std::move(shader));
		} else {
			_status = Status::CompilationFailed;
		}
		return hasCompiled;
	}

	bool GLShaderProgram::Link(Introspection introspection)
	{
#if defined(WITH_AMIGA_OGLES2)
		// NCINE_DUMPSHADERS=<dir>: write each linked program's sources as <n>.vert/<n>.frag, the input of
		// ogles2's offline compiler (v3dglslc), so a release can ship the binaries
		if (const char* dumpDir = std::getenv("NCINE_DUMPSHADERS")) {
			static std::int32_t dumpIndex = 0;
			dumpIndex++;
			for (auto& shader : _attachedShaders) {
				char path[512];
				std::snprintf(path, sizeof(path), "%s/%03d.%s", dumpDir, dumpIndex, shader->IsVertex ? "vert" : "frag");
				if (FILE* f = std::fopen(path, "wb")) {
					std::fwrite(shader->DumpSource.data(), 1, shader->DumpSource.size(), f);
					std::fclose(f);
				}
			}
		}
#endif
		glLinkProgram(_glHandle);
		return FinalizeAfterLinking(introspection);
	}

	void GLShaderProgram::Use()
	{
		if (_boundProgram != _glHandle) {
			ProcessDeferredQueries();

			glUseProgram(_glHandle);
			_boundProgram = _glHandle;
		}
	}

	bool GLShaderProgram::Validate()
	{
#if defined(DEATH_TARGET_VITA)
		// vitaGL provides no glValidateProgram(), program validation is a debug-only aid, so assume success
		return true;
#else
		glValidateProgram(_glHandle);
		GLint status;
		glGetProgramiv(_glHandle, GL_VALIDATE_STATUS, &status);
		return (status == GL_TRUE);
#endif
	}

	bool GLShaderProgram::FinalizeAfterLinking(Introspection introspection)
	{
		_introspection = introspection;

		if (_queryPhase == QueryPhase::Immediate) {
			_status = Status::NotLinked;
			const bool linkCheck = CheckLinking();
			if (!linkCheck) {
				return false;
			}

			// After linking, shader objects are not needed anymore
#if !defined(DEATH_TARGET_VITA)
			// vitaGL has no glDetachShader(), the shader objects are released with the program instead
			for (auto& shader : _attachedShaders) {
				glDetachShader(_glHandle, shader->GetGLHandle());
			}
#endif

			_attachedShaders.clear();

			PerformIntrospection();
			return linkCheck;
		} else {
			_status = GLShaderProgram::Status::LinkedWithDeferredQueries;
			return true;
		}
	}

	GLVertexFormat::Attribute* GLShaderProgram::GetAttribute(const char* name)
	{
		DEATH_ASSERT(name);
		GLVertexFormat::Attribute* vertexAttribute = nullptr;

		std::int32_t location = -1;
		const bool attributeFound = _attributeLocations.contains(name, location);
		if (attributeFound) {
			vertexAttribute = &_vertexFormat[location];
		}
		return vertexAttribute;
	}

	void GLShaderProgram::DefineVertexFormat(const GLBufferObject* vbo, const GLBufferObject* ibo, std::uint32_t vboOffset)
	{
#if defined(RHI_GL_PROFILE_ES2)
		// ES2: the gl_VertexID-free sprite/full-screen shaders read their quad corner from the shared static
		// corner VBO (bound to aQuadCorner); any real geometry attributes (mesh sprites) come from the geometry
		// VBO. A VAO must be bound even when the sprite has no geometry VBO, so drive it off attribute presence.
		std::int32_t cornerLocation = -1;
		_attributeLocations.contains(Material::QuadCornerAttributeName, cornerLocation);
		const GLBufferObject* cornerVbo = static_cast<const GLBufferObject*>(RenderResources::GetQuadCornerVbo());
#	if defined(WITH_AMIGA_OGLES2)
		std::int32_t instanceLocation = -1;
		if (_attributeLocations.contains(Material::InstanceIndexAttributeName, instanceLocation)) {
			cornerVbo = static_cast<const GLBufferObject*>(RenderResources::GetBatchedQuadCornerVbo());
		}
#	else
		const std::int32_t instanceLocation = -1;
#	endif
		bool hasBoundAttribute = false;
		for (std::int32_t location : _attributeLocations) {
			if (location == cornerLocation || location == instanceLocation) {
				if (cornerVbo != nullptr) {
					_vertexFormat[location].setVbo(cornerVbo);
					_vertexFormat[location].SetBaseOffset(0);
					hasBoundAttribute = true;
				}
			} else if (vbo != nullptr) {
				_vertexFormat[location].setVbo(vbo);
				_vertexFormat[location].SetBaseOffset(vboOffset);
				hasBoundAttribute = true;
			}
		}
		_vertexFormat.SetIbo(ibo);
		if (hasBoundAttribute) {
			RenderResources::GetVaoPool().BindVao(_vertexFormat);
		}
#else
		if (vbo != nullptr) {
			for (std::int32_t location : _attributeLocations) {
				_vertexFormat[location].setVbo(vbo);
				_vertexFormat[location].SetBaseOffset(vboOffset);
			}
			_vertexFormat.SetIbo(ibo);

#if !defined(WITH_RHI_SOFTWARE)
			RenderResources::GetVaoPool().BindVao(_vertexFormat);
#endif
		}
#endif
	}

	void GLShaderProgram::Reset()
	{
		if (_status != Status::NotLinked && _status != Status::CompilationFailed) {
			_uniforms.clear();
			_uniformBlocks.clear();
			_attributes.clear();

			_attributeLocations.clear();
			_vertexFormat.Reset();

			if (_boundProgram == _glHandle) {
				glUseProgram(0);
			}

#if !defined(DEATH_TARGET_VITA)
			// vitaGL has no glDetachShader(), the shader objects are released with the program instead
			for (auto& shader : _attachedShaders) {
				glDetachShader(_glHandle, shader->GetGLHandle());
			}
#endif

			_attachedShaders.clear();
			glDeleteProgram(_glHandle);

#if !defined(WITH_RHI_SOFTWARE)
			RenderResources::RemoveCameraUniformData(this);
			RenderResources::UnregisterBatchedShader(this);
#endif

			_glHandle = glCreateProgram();
		}

		_status = Status::NotLinked;
		_batchSize = DefaultBatchSize;
		_reflection = nullptr;
	}

	void GLShaderProgram::SetObjectLabel(StringView label)
	{
		GLDebug::SetObjectLabel(GLDebug::LabelTypes::Program, _glHandle, label);
	}

	bool GLShaderProgram::ProcessDeferredQueries()
	{
		if (_status == GLShaderProgram::Status::LinkedWithDeferredQueries) {
			for (std::unique_ptr<GLShader>& attachedShader : _attachedShaders) {
				const bool compileCheck = attachedShader->CheckCompilation(_shouldLogOnErrors);
				if (!compileCheck) {
					return false;
				}
			}

			const bool linkCheck = CheckLinking();
			if (!linkCheck) {
				return false;
			}

			// After linking, shader objects are not needed anymore
#if !defined(DEATH_TARGET_VITA)
			// vitaGL has no glDetachShader(), the shader objects are released with the program instead
			for (auto& shader : _attachedShaders) {
				glDetachShader(_glHandle, shader->GetGLHandle());
			}
#endif

			_attachedShaders.clear();

			PerformIntrospection();
		}
		return true;
	}

	bool GLShaderProgram::CheckLinking()
	{
		if (_status == Status::Linked || _status == Status::LinkedWithIntrospection) {
			return true;
		}

		GLint status;
		glGetProgramiv(_glHandle, GL_LINK_STATUS, &status);
		if (status == GL_FALSE) {
#if defined(DEATH_TRACE)
			if (_shouldLogOnErrors) {
				GLint length = 0;
				glGetProgramiv(_glHandle, GL_INFO_LOG_LENGTH, &length);
				if (length > 0) {
					static char buffer[2048];
					glGetProgramInfoLog(_glHandle, sizeof(buffer), &length, buffer);
					LOGW("{}", buffer);
				}
			}
#endif
			_status = Status::LinkingFailed;
			return false;
		}

		_status = Status::Linked;
		return true;
	}

	void GLShaderProgram::PerformIntrospection()
	{
		if (_introspection != Introspection::Disabled && _status != Status::LinkedWithIntrospection) {
			_uniformsSize = 0;
			_uniformBlocksSize = 0;

			if (_reflection != nullptr) {
				ImportReflection();
			} else {
				const GLUniformBlock::DiscoverUniforms discover = (_introspection == Introspection::NoUniformsInBlocks)
					? GLUniformBlock::DiscoverUniforms::Disabled
					: GLUniformBlock::DiscoverUniforms::Enabled;

				DiscoverUniforms();
				DiscoverUniformBlocks(discover);
				DiscoverAttributes();
			}
			InitVertexFormat();
			_status = Status::LinkedWithIntrospection;
		}
		// The reflection data is consumed by introspection and may not outlive the caller, so it is never kept
		_reflection = nullptr;
	}

	void GLShaderProgram::ImportReflection()
	{
		ZoneScopedC(0x81A861);
		const ShaderCompiler::ProgramVariant& reflection = *_reflection;

		// Loose uniforms - the reflection keeps samplers in a separate list, but GL treats them as uniforms
		// (all samplers are 2D across the shader set; the reflected texture bindings carry no dimension)
		for (std::size_t i = 0; i < reflection.UniformCount; i++) {
			const ShaderCompiler::Uniform& u = reflection.Uniforms[i];
			GLUniform& uniform = _uniforms.emplace_back(_glHandle, u.Name, UniformTypeToGL(u.Type), GLint(u.ArraySize));
			_uniformsSize += uniform.GetMemorySize();

			LOGD("Shader program {} - uniform {} : \"{}\" (reflected)", _glHandle, uniform.GetLocation(), uniform.GetName());
		}
		for (std::size_t i = 0; i < reflection.TextureCount; i++) {
			const ShaderCompiler::TextureBinding& t = reflection.Textures[i];
			GLUniform& uniform = _uniforms.emplace_back(_glHandle, t.Name, GL_SAMPLER_2D, 1);
			_uniformsSize += uniform.GetMemorySize();

			LOGD("Shader program {} - sampler uniform {} : \"{}\" (reflected)", _glHandle, uniform.GetLocation(), uniform.GetName());
		}

		for (std::size_t i = 0; i < reflection.BlockCount; i++) {
			const ShaderCompiler::UniformBlock& b = reflection.Blocks[i];
#if defined(RHI_GL_PROFILE_ES2)
			// ES2 (ESSL 100) has no uniform buffer objects: the block's members are plain loose uniforms in
			// the linked program. Keep the block in the reflection so BaseSprite/Material still address the
			// per-instance data through it, but resolve each member's real loose location below so that
			// GLShaderUniformBlocks::Bind() can push them with glUniform* instead of a UBO bind. There is no
			// glGetUniformBlockIndex() (ES 3.0) to call; the stored index stays GL_INVALID_INDEX and unused
			// (the ES2 path never binds a buffer range).
			const GLuint blockIndex = GL_INVALID_INDEX;
#else
			const GLuint blockIndex = glGetUniformBlockIndex(_glHandle, b.Name);
			if (blockIndex == GL_INVALID_INDEX) {
				// The whole block was optimized out by the driver - GL introspection would not have listed it either
				LOGD("Shader program {} - uniform block \"{}\" is inactive and was skipped", _glHandle, b.Name);
				continue;
			}
#endif

			// A BATCH_SIZE-sized instance array uses the explicitly set batch size, or the same 64 KB-based
			// fallback the in-shader "#ifndef BATCH_SIZE" defaults assume when no size is injected
			std::uint32_t effectiveBatchSize = 0;
			std::uint32_t dataSize = b.BaseSize;
			if (b.InstanceStride > 0) {
				effectiveBatchSize = (_batchSize != std::uint32_t(DefaultBatchSize) && _batchSize > 0)
					? _batchSize : (64 * 1024) / b.InstanceStride;
				dataSize += b.InstanceStride * effectiveBatchSize;
			}

			GLUniformBlock& uniformBlock = _uniformBlocks.emplace_back(_glHandle, b.Name, blockIndex, GLint(dataSize));
			_uniformBlocksSize += uniformBlock.GetSize();
#if defined(WITH_AMIGA_OGLES2)
			// The instance array of a batched program was split per field when it was compiled (see
			// FlattenInstanceArray) - read the layout from the same ES2 text, which also covers a program
			// restored from the binary cache without its sources
			if (b.InstanceStride > 0 && reflection.VsSource100 != nullptr) {
				const std::string vsSource(reflection.VsSource100);
				std::string typeName;
				std::size_t lineStart, lineEnd;
				std::uint32_t stride = 0;
				if (vsSource.find("instances[int(aInstanceIndex)]") != std::string::npos &&
					ParseInstanceArray(vsSource, typeName, lineStart, lineEnd, uniformBlock.FlatFields, uniformBlock.FlatFieldCount, stride)) {
					if (stride != b.InstanceStride) {
						LOGW("Shader program {} - instance stride {} does not match the reflected {}, not batching it", _glHandle, stride, b.InstanceStride);
						uniformBlock.FlatFieldCount = 0;
					} else {
						uniformBlock.FlatStride = stride;
						for (std::uint32_t f = 0; f < uniformBlock.FlatFieldCount; f++) {
							char name[48];
							std::snprintf(name, sizeof(name), "instances_%s", uniformBlock.FlatFields[f].Name);
							uniformBlock.FlatFields[f].Location = glGetUniformLocation(_glHandle, name);
						}
					}
				} else {
					uniformBlock.FlatFieldCount = 0;
				}
			}
#endif

			if (_introspection != Introspection::NoUniformsInBlocks) {
				for (std::size_t j = 0; j < b.MemberCount; j++) {
					const ShaderCompiler::BlockMember& m = b.Members[j];
					if (m.Type == ShaderCompiler::UniformType::Struct) {
						// GL introspection reports flattened leaves of struct members instead - no engine code
						// accesses struct aggregates by name, so they are skipped here
						continue;
					}

					GLUniform blockUniform;
					std::size_t nameLength = strnlen(m.Name, GLUniform::MaxNameLength);
					DEATH_ASSERT(nameLength < GLUniform::MaxNameLength);
					std::memcpy(blockUniform._name, m.Name, nameLength);
					blockUniform._name[nameLength] = '\0';
					blockUniform._blockIndex = GLint(blockIndex);
					blockUniform._type = UniformTypeToGL(m.Type);
					blockUniform._size = (m.ArraySize == ShaderCompiler::SymbolicArraySize) ? GLint(effectiveBatchSize)
						: (m.ArraySize > 0 ? GLint(m.ArraySize) : 1);
					blockUniform._offset = GLint(m.Offset);
#if defined(RHI_GL_PROFILE_ES2)
					// The member is a real loose uniform on ES2 - resolve its location for the glUniform upload
					blockUniform._location = glGetUniformLocation(_glHandle, m.Name);
#endif
					uniformBlock._blockUniforms[blockUniform._name] = blockUniform;
				}
			}

			LOGD("Shader program {} - uniform block {} : \"{}\" ({} bytes with {} align, reflected)", _glHandle, uniformBlock.GetIndex(), uniformBlock.GetName(), uniformBlock.GetSize(), uniformBlock.GetAlignAmount());
		}

#if defined(RHI_GL_PROFILE_ES2)
		// The reflected attributes describe the modern (gl_VertexID) source; the linked ES2 program has
		// different vertex inputs (aQuadCorner / aInstanceIndex replacing gl_VertexID), so query the real
		// active attributes from GL instead of the offline reflection.
		DiscoverAttributes();
#else
		for (std::size_t i = 0; i < reflection.AttributeCount; i++) {
			const ShaderCompiler::Attribute& a = reflection.Attributes[i];
			DEATH_UNUSED GLAttribute& attribute = _attributes.emplace_back(_glHandle, a.Name, UniformTypeToGL(a.Type));
			LOGD("Shader program {} - attribute {} : \"{}\" (reflected)", _glHandle, attribute.GetLocation(), attribute.GetName());
		}
#endif
		GL_LOG_ERRORS();
	}

	void GLShaderProgram::DiscoverUniforms()
	{
		ZoneScopedC(0x81A861);
#if defined(RHI_GL_PROFILE_ES2)
		// ES2 has no uniform blocks, so every active uniform is a loose one - enumerate them directly
		// (glGetActiveUniformsiv() used below to filter out block members is ES 3.0)
		GLint uniformCount = 0;
		glGetProgramiv(_glHandle, GL_ACTIVE_UNIFORMS, &uniformCount);

		for (GLint i = 0; i < uniformCount; i++) {
			GLUniform& uniform = _uniforms.emplace_back(_glHandle, static_cast<GLuint>(i));
			_uniformsSize += uniform.GetMemorySize();

			LOGD("Shader program {} - uniform {} : \"{}\"", _glHandle, uniform.GetLocation(), uniform.GetName());
		}
#else
		static const std::uint32_t NumIndices = 512;
		static GLuint uniformIndices[NumIndices];
		static GLint blockIndices[NumIndices];

		GLint uniformCount = 0;
		glGetProgramiv(_glHandle, GL_ACTIVE_UNIFORMS, &uniformCount);

		if (uniformCount > 0) {
			std::uint32_t uniformsOutsideBlocks = 0;
			GLuint indices[MaxNumUniforms];
			std::uint32_t remainingIndices = static_cast<std::uint32_t>(uniformCount);

			while (remainingIndices > 0) {
				const std::uint32_t uniformCountStep = (remainingIndices > NumIndices) ? NumIndices : remainingIndices;
				const std::uint32_t startIndex = static_cast<std::uint32_t>(uniformCount) - remainingIndices;

				for (std::uint32_t i = 0; i < uniformCountStep; i++)
					uniformIndices[i] = startIndex + i;

				glGetActiveUniformsiv(_glHandle, uniformCountStep, uniformIndices, GL_UNIFORM_BLOCK_INDEX, blockIndices);

				for (std::uint32_t i = 0; i < uniformCountStep; i++) {
					if (blockIndices[i] == -1) {
						indices[uniformsOutsideBlocks] = startIndex + i;
						uniformsOutsideBlocks++;
					}
					if (uniformsOutsideBlocks > MaxNumUniforms - 1)
						break;
				}

				remainingIndices -= uniformCountStep;
			}

			for (std::uint32_t i = 0; i < uniformsOutsideBlocks; i++) {
				GLUniform& uniform = _uniforms.emplace_back(_glHandle, indices[i]);
				_uniformsSize += uniform.GetMemorySize();

				LOGD("Shader program {} - uniform {} : \"{}\"", _glHandle, uniform.GetLocation(), uniform.GetName());
			}
		}
#endif
		GL_LOG_ERRORS();
	}

	void GLShaderProgram::DiscoverUniformBlocks(GLUniformBlock::DiscoverUniforms discover)
	{
#if defined(RHI_GL_PROFILE_ES2)
		// ES2 has no uniform blocks (GL_ACTIVE_UNIFORM_BLOCKS is ES 3.0); raw-string programs on this profile
		// (e.g. ImGui) only carry loose uniforms, which DiscoverUniforms() has already enumerated
		static_cast<void>(discover);
#else
		ZoneScopedC(0x81A861);
		GLint count;
		glGetProgramiv(_glHandle, GL_ACTIVE_UNIFORM_BLOCKS, &count);

		for (std::int32_t i = 0; i < count; i++) {
			GLUniformBlock& uniformBlock = _uniformBlocks.emplace_back(_glHandle, i, discover);
			_uniformBlocksSize += uniformBlock.GetSize();

			LOGD("Shader program {} - uniform block {} : \"{}\" ({} bytes with {} align)", _glHandle, uniformBlock.GetIndex(), uniformBlock.GetName(), uniformBlock.GetSize(), uniformBlock.GetAlignAmount());
		}
		GL_LOG_ERRORS();
#endif
	}

	void GLShaderProgram::DiscoverAttributes()
	{
		ZoneScopedC(0x81A861);
		GLint count;
		glGetProgramiv(_glHandle, GL_ACTIVE_ATTRIBUTES, &count);

		for (std::int32_t i = 0; i < count; i++) {
			DEATH_UNUSED GLAttribute& attribute = _attributes.emplace_back(_glHandle, i);
			LOGD("Shader program {} - attribute {} : \"{}\"", _glHandle, attribute.GetLocation(), attribute.GetName());
		}
		GL_LOG_ERRORS();
	}

	void GLShaderProgram::InitVertexFormat()
	{
		ZoneScopedC(0x81A861);
		const std::uint32_t count = (std::uint32_t)_attributes.size();
		if (count > GLVertexFormat::MaxAttributes) {
			LOGW("More active attributes ({}) than supported by the vertex format class ({})", count, GLVertexFormat::MaxAttributes);
		}
		for (std::uint32_t i = 0; i < _attributes.size(); i++) {
			const GLAttribute& attribute = _attributes[i];
			const std::int32_t location = attribute.GetLocation();
			if (location < 0)
				continue;

			_attributeLocations[attribute.GetName()] = location;
			_vertexFormat[location].Init(attribute.GetLocation(), attribute.GetComponentCount(), attribute.GetBasicType());
#if defined(RHI_GL_PROFILE_ES2)
			// The ES2 quad-corner attribute is fed from a tightly-packed static VBO (a plain vec2 stream),
			// not the geometry VBO; set its stride/offset once here (the VBO is bound in DefineVertexFormat).
			if (std::strcmp(attribute.GetName(), Material::QuadCornerAttributeName) == 0) {
#	if defined(WITH_AMIGA_OGLES2)
				// Batched programs read corner + instance index from one interleaved stream (x, y, index)
				_vertexFormat[location].SetVboParameters(GLsizei((HasInstanceIndexAttribute() ? 3 : 2) * sizeof(GLfloat)), nullptr);
#	else
				_vertexFormat[location].SetVboParameters(2 * sizeof(GLfloat), nullptr);
#	endif
			}
#	if defined(WITH_AMIGA_OGLES2)
			else if (std::strcmp(attribute.GetName(), Material::InstanceIndexAttributeName) == 0) {
				_vertexFormat[location].SetVboParameters(3 * sizeof(GLfloat), reinterpret_cast<const GLvoid*>(2 * sizeof(GLfloat)));
			}
#	endif
#endif
		}
	}
}
