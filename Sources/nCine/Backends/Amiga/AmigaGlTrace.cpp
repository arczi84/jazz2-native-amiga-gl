// DIAG: NCINE_GLSKIP="combine additive repeat linear readpixels bigquad depthclear" alters single GL features on the way to the
// driver, to find which one a driver regression depends on (see Install()).
// DIAG: records every GL call the backend makes for two frames, to hand a driver author an exact call
// sequence. Enabled with NCINE_GLTRACE in the environment, armed with Ctrl-D (`Break <pid> D`), written to
// RAM:gltrace.txt. Without NCINE_GLTRACE nothing is installed and the dispatch table is the library's own.
#if defined(WITH_AMIGA) && defined(AMIGA_SHARED_MINIGL)

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <proto/minigl.h>
#include <mgl/gl.h>
#include <libraries/minigl_dispatch.h>
#include <dos/dos.h>
#include <proto/exec.h>


namespace nCine::Backends::AmigaGlTrace
{
	static MGLDispatchTable traceTable;
	static MGLDispatchTable realTable;
	static char* buffer = nullptr;
	static std::size_t length = 0;
	static constexpr std::size_t Capacity = 1024 * 1024;
	static int framesLeft = 0;
	static int frameNumber = 0;

	static void Append(const char* format, ...)
	{
		if (framesLeft <= 0 || buffer == nullptr || length + 256 >= Capacity) {
			return;
		}
		va_list args;
		va_start(args, format);
		int n = std::vsnprintf(buffer + length, Capacity - length, format, args);
		va_end(args);
		if (n > 0) {
			length += std::size_t(n);
		}
	}

	static int F(double v) { return int(v * 1000.0); }

	static bool skipCombine = false, skipAdditive = false, skipRepeat = false, skipLinear = false, skipReadPixels = false, skipBigQuad = false, skipDepthClear = false;
	static int viewportW = 0, viewportH = 0;

	static void TrActiveTextureARB(GLcontext c, GLenum u) { Append("glActiveTextureARB(0x%x)\n", u); realTable.GLActiveTextureARB(c, u); }
	static void TrBindTexture(GLcontext c, GLenum t, GLuint tex) { Append("glBindTexture(0x%x, %u)\n", t, tex); realTable.GLBindTexture(c, t, tex); }
	static void TrBlendFunc(GLcontext c, GLenum s, GLenum d) { if (skipAdditive && d == 0x1) d = 0x303; Append("glBlendFunc(0x%x, 0x%x)\n", s, d); realTable.GLBlendFunc(c, s, d); }
	static void TrClear(GLcontext c, GLbitfield m) { if (skipDepthClear) m &= ~GLbitfield(0x100 | 0x400); Append("glClear(0x%x)\n", m); if (m != 0) realTable.GLClear(c, m); }
	static void TrClearColor(GLcontext c, GLclampf r, GLclampf g, GLclampf b, GLclampf a) { Append("glClearColor(%d, %d, %d, %d) /1000\n", F(r), F(g), F(b), F(a)); realTable.GLClearColor(c, r, g, b, a); }
	static void TrColorPointer(GLcontext c, GLint s, GLenum t, GLsizei st, const GLvoid* p) { Append("glColorPointer(%d, 0x%x, %d, %p)\n", s, t, st, p); realTable.GLColorPointer(c, s, t, st, p); }
	static void TrDepthMask(GLcontext c, GLboolean f) { Append("glDepthMask(%d)\n", f); realTable.GLDepthMask(c, f); }
	static void TrSetState(GLcontext c, GLenum cap, GLboolean on) { Append("%s(0x%x)\n", on ? "glEnable" : "glDisable", cap); realTable.MGLSetState(c, cap, on); }
	static void TrEnableClientState(GLcontext c, GLenum cap) { Append("glEnableClientState(0x%x)\n", cap); realTable.GLEnableClientState(c, cap); }
	static void TrLoadIdentity(GLcontext c) { Append("glLoadIdentity()\n"); realTable.GLLoadIdentity(c); }
	static void TrMatrixMode(GLcontext c, GLenum m) { Append("glMatrixMode(0x%x)\n", m); realTable.GLMatrixMode(c, m); }
	static void TrOrtho(GLcontext c, GLdouble l, GLdouble r, GLdouble b, GLdouble t, GLdouble n, GLdouble f) { Append("glOrtho(%d, %d, %d, %d, %d, %d) /1000\n", F(l), F(r), F(b), F(t), F(n), F(f)); realTable.GLOrtho(c, l, r, b, t, n, f); }
	static void TrPixelStorei(GLcontext c, GLenum p, GLint v) { Append("glPixelStorei(0x%x, %d)\n", p, v); realTable.GLPixelStorei(c, p, v); }
	static void TrReadPixels(GLcontext c, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum t, GLvoid* p) { Append("glReadPixels(%d, %d, %d, %d, 0x%x, 0x%x)%s\n", x, y, w, h, f, t, skipReadPixels ? " SKIPPED" : ""); if (skipReadPixels) { std::memset(p, 0x80, std::size_t(w) * std::size_t(h) * 4); return; } realTable.GLReadPixels(c, x, y, w, h, f, t, p); }
	static void TrScalef(GLcontext c, GLfloat x, GLfloat y, GLfloat z) { Append("glScalef(%d, %d, %d) /1000\n", F(x), F(y), F(z)); realTable.GLScalef(c, x, y, z); }
	static void TrScissor(GLcontext c, GLint x, GLint y, GLsizei w, GLsizei h) { Append("glScissor(%d, %d, %d, %d)\n", x, y, w, h); realTable.GLScissor(c, x, y, w, h); }
	static const GLvoid* texCoordPointer = nullptr;
	static void TrTexCoordPointer(GLcontext c, GLint s, GLenum t, GLsizei st, const GLvoid* p) { Append("glTexCoordPointer(%d, 0x%x, %d, %p)\n", s, t, st, p); texCoordPointer = p; realTable.GLTexCoordPointer(c, s, t, st, p); }
	static void TrTexEnvi(GLcontext c, GLenum t, GLenum p, GLint v)
	{
		if (skipCombine) {
			if (p == 0x2200 && v == 0x8570) v = 0x2100;	// GL_TEXTURE_ENV_MODE GL_COMBINE -> GL_MODULATE
			else if ((p >= 0x8571 && p <= 0x859A) || p == 0x0D1C) return;	// combiner parameters, alpha scale
		}
		Append("glTexEnvi(0x%x, 0x%x, 0x%x)\n", t, p, v);
		realTable.GLTexEnvi(c, t, p, v);
	}
	static void TrTexImage2D(GLcontext c, GLenum t, GLint l, GLint ifmt, GLsizei w, GLsizei h, GLint b, GLenum f, GLenum ty, const GLvoid* p) { Append("glTexImage2D(0x%x, %d, 0x%x, %d, %d, %d, 0x%x, 0x%x, %s)\n", t, l, ifmt, w, h, b, f, ty, p ? "data" : "NULL"); realTable.GLTexImage2D(c, t, l, ifmt, w, h, b, f, ty, p); }
	static void TrTexParameteri(GLcontext c, GLenum t, GLenum p, GLint v)
	{
		if (skipRepeat && (p == 0x2802 || p == 0x2803) && v == 0x2901) v = 0x812F;	// GL_REPEAT -> GL_CLAMP_TO_EDGE
		if (skipLinear && (p == 0x2800 || p == 0x2801) && v == 0x2601) v = 0x2600;	// GL_LINEAR -> GL_NEAREST
		Append("glTexParameteri(0x%x, 0x%x, 0x%x)\n", t, p, v);
		realTable.GLTexParameteri(c, t, p, v);
	}
	static void TrTexSubImage2D(GLcontext c, GLenum t, GLint l, GLint x, GLint y, GLsizei w, GLsizei h, GLenum f, GLenum ty, const GLvoid* p) { Append("glTexSubImage2D(0x%x, %d, %d, %d, %d, %d, 0x%x, 0x%x)\n", t, l, x, y, w, h, f, ty); realTable.GLTexSubImage2D(c, t, l, x, y, w, h, f, ty, p); }
	static void TrVertexPointer(GLcontext c, GLint s, GLenum t, GLsizei st, const GLvoid* p) { Append("glVertexPointer(%d, 0x%x, %d, %p)\n", s, t, st, p); realTable.GLVertexPointer(c, s, t, st, p); }
	static void TrViewport(GLcontext c, GLint x, GLint y, GLsizei w, GLsizei h) { Append("glViewport(%d, %d, %d, %d)\n", x, y, w, h); viewportW = w; viewportH = h; realTable.GLViewport(c, x, y, w, h); }

	// Draws log their first vertices too, so the coordinates the driver receives are on record
	static void TrDrawArrays(GLcontext c, GLenum m, GLint first, GLsizei count)
	{
		if (skipBigQuad && texCoordPointer != nullptr && viewportW > 0) {
			// Triangles reaching more than half a viewport outside it (the menu's rotated background layers)
			for (GLsizei i = 0; i < count; i++) {
				float xyz[3];
				std::memcpy(xyz, static_cast<const unsigned char*>(texCoordPointer) + std::size_t(first + i) * 24 + 12, sizeof(xyz));
				if (xyz[0] < -viewportW * 0.5f || xyz[0] > viewportW * 1.5f || xyz[1] < -viewportH * 0.5f || xyz[1] > viewportH * 1.5f) {
					Append("glDrawArrays(0x%x, %d, %d) SKIPPED (big quad)\n", m, first, count);
					return;
				}
			}
		}
		Append("glDrawArrays(0x%x, %d, %d)\n", m, first, count);
		// GL_T2F_C4UB_V3F: s t (floats), rgba (bytes), x y z (floats) - the format the backend submits
		// The backend submits GL_T2F_C4UB_V3F records through three pointers: s t, rgba, x y z (24 bytes)
		if (texCoordPointer != nullptr) {
			const GLsizei stride = 24;
			for (GLsizei i = 0; i < count && i < 4; i++) {
				const unsigned char* v = static_cast<const unsigned char*>(texCoordPointer) + std::size_t(first + i) * std::size_t(stride);
				float st[2], xyz[3];
				std::memcpy(st, v, sizeof(st));
				std::memcpy(xyz, v + 12, sizeof(xyz));
				Append("    v%d: st=(%d, %d) rgba=%02x%02x%02x%02x xyz=(%d, %d, %d) /1000\n", int(i), F(st[0]), F(st[1]), v[8], v[9], v[10], v[11], F(xyz[0]), F(xyz[1]), F(xyz[2]));
			}
		}
		realTable.GLDrawArrays(c, m, first, count);
	}

	void Install()
	{
		const char* skip = std::getenv("NCINE_GLSKIP");
		if ((std::getenv("NCINE_GLTRACE") == nullptr && skip == nullptr) || MiniGLDispatch == nullptr) {
			return;
		}
		if (skip != nullptr) {
			skipCombine = (std::strstr(skip, "combine") != nullptr);
			skipAdditive = (std::strstr(skip, "additive") != nullptr);
			skipRepeat = (std::strstr(skip, "repeat") != nullptr);
			skipLinear = (std::strstr(skip, "linear") != nullptr);
			skipReadPixels = (std::strstr(skip, "readpixels") != nullptr);
			skipBigQuad = (std::strstr(skip, "bigquad") != nullptr);
			skipDepthClear = (std::strstr(skip, "depthclear") != nullptr);
			std::fprintf(stdout, "nCine: NCINE_GLSKIP=\"%s\" (combine %d, additive %d, repeat %d, linear %d, readpixels %d, bigquad %d)\n",
				skip, skipCombine, skipAdditive, skipRepeat, skipLinear, skipReadPixels, skipBigQuad);
		}
		buffer = static_cast<char*>(std::malloc(Capacity));
		if (buffer == nullptr) {
			return;
		}
		std::size_t size = MiniGLDispatch->structSize;
		if (size > sizeof(MGLDispatchTable)) {
			size = sizeof(MGLDispatchTable);
		}
		std::memcpy(&realTable, MiniGLDispatch, size);
		std::memcpy(&traceTable, MiniGLDispatch, size);
		traceTable.GLActiveTextureARB = TrActiveTextureARB;
		traceTable.GLBindTexture = TrBindTexture;
		traceTable.GLBlendFunc = TrBlendFunc;
		traceTable.GLClear = TrClear;
		traceTable.GLClearColor = TrClearColor;
		traceTable.GLColorPointer = TrColorPointer;
		traceTable.GLDepthMask = TrDepthMask;
		traceTable.GLDrawArrays = TrDrawArrays;
		traceTable.MGLSetState = TrSetState;
		traceTable.GLEnableClientState = TrEnableClientState;
		traceTable.GLLoadIdentity = TrLoadIdentity;
		traceTable.GLMatrixMode = TrMatrixMode;
		traceTable.GLOrtho = TrOrtho;
		traceTable.GLPixelStorei = TrPixelStorei;
		traceTable.GLReadPixels = TrReadPixels;
		traceTable.GLScalef = TrScalef;
		traceTable.GLScissor = TrScissor;
		traceTable.GLTexCoordPointer = TrTexCoordPointer;
		traceTable.GLTexEnvi = TrTexEnvi;
		traceTable.GLTexImage2D = TrTexImage2D;
		traceTable.GLTexParameteri = TrTexParameteri;
		traceTable.GLTexSubImage2D = TrTexSubImage2D;
		traceTable.GLVertexPointer = TrVertexPointer;
		traceTable.GLViewport = TrViewport;
		MiniGLDispatch = &traceTable;
		std::fprintf(stderr, "nCine: GL trace installed, arm it with Ctrl-D\n");
	}

	void EndOfFrame(int rasterWidth, int rasterHeight, int logicalWidth, int logicalHeight)
	{
		if (buffer == nullptr) {
			return;
		}
		frameNumber++;
		if (framesLeft > 0) {
			Append("=== end of frame %d\n", frameNumber);
			if (--framesLeft == 0) {
				if (FILE* f = std::fopen("RAM:gltrace.txt", "w")) {
					std::fwrite(buffer, 1, length, f);
					std::fclose(f);
				}
				std::fprintf(stderr, "nCine: GL trace written to RAM:gltrace.txt (%u bytes)\n", unsigned(length));
			}
			return;
		}
		if ((SetSignal(0, SIGBREAKF_CTRL_D) & SIGBREAKF_CTRL_D) != 0) {
			length = 0;
			framesLeft = 2;
			Append("=== GL trace: frame %d, raster %dx%d, logical %dx%d, structSize %u\n", frameNumber + 1,
				rasterWidth, rasterHeight, logicalWidth, logicalHeight, unsigned(realTable.structSize));
		}
	}
}

#endif
