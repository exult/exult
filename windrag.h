#ifndef INCL_WINDRAG
#define INCL_WINDRAG

#if defined(_WIN32) && defined(USE_EXULTSTUDIO)

#	ifdef __GNUC__
#		ifdef __clang__
// Working around this clang bug with pragma push/pop:
// https://github.com/clangd/clangd/issues/1167
static_assert(true);
#		endif
// COM sucks.
#		pragma GCC diagnostic push
#		pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#		pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	endif

#	include "u7drag.h"

#	include <ole2.h>

#	include <atomic>

/*
 * The 'IDropTarget' implementation
 */
class FAR Windnd final : public IDropTarget {
private:
	HWND gamewin;

	std::atomic<DWORD> m_cRef;

	Move_shape_handler_fun move_shape_handler;
	Move_combo_handler_fun move_combo_handler;
	Drop_shape_handler_fun shape_handler;
	Drop_chunk_handler_fun chunk_handler;
	Drop_npc_handler_fun   npc_handler;
	Drop_combo_handler_fun combo_handler;

	// Used for when dragging the mouse over the exult window
	int drag_id;
	int prevx, prevy;

	union {
		struct {
			int file, shape, frame;
		} shape;

		struct {
			int chunknum;
		} chunk;

		struct {
			int            xtiles, ytiles;
			int            right, below, cnt;
			U7_combo_data* combo;
		} combo;

		struct {
			int npcnum;
		} npc;
	} data;

	~Windnd() = default;

public:
	Windnd(HWND hgwnd, Move_shape_handler_fun, Move_combo_handler_fun,
		   Drop_shape_handler_fun, Drop_chunk_handler_fun,
		   Drop_npc_handler_fun npcfun, Drop_combo_handler_fun);

	STDMETHOD(QueryInterface)(REFIID iid, void** ppvObject) override;
	STDMETHOD_(ULONG, AddRef)() override;
	STDMETHOD_(ULONG, Release)() override;

	STDMETHOD(DragEnter)
	(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
	 DWORD* pdwEffect) override;
	STDMETHOD(DragOver)
	(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) override;
	STDMETHOD(DragLeave)() override;
	STDMETHOD(Drop)
	(IDataObject* pDataObject, DWORD grfKeyState, POINTL pt,
	 DWORD* pdwEffect) override;

	bool is_valid(IDataObject* pDataObject);
};

#	ifdef __GNUC__
#		pragma GCC diagnostic pop
#	endif

#endif

#endif
