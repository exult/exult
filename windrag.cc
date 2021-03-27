/*
* This file implements some Win32 OLE2 specific Drag and Drop structures.
* Most of the code is a modification of the OLE2 SDK sample programs
* "drgdrps" and "drgdrpt". If you want to see them, don't search at
* Microsoft (I didn't find them there, seems they reorganised their servers)
* but instead at
*  http://ftp.se.kde.org/pub/vendor/microsoft/Softlib/MSLFILES/
*/
#if defined(_WIN32) && defined(USE_EXULTSTUDIO)

#include <iostream>

#include "windrag.h"
#include "u7drag.h"
#include "ignore_unused_variable_warning.h"

static UINT CF_EXULT = RegisterClipboardFormat(U7_TARGET_GENERIC_NAME_X11);

// Just in case
#ifndef DECLSPEC_NOTHROW
#define DECLSPEC_NOTHROW
#endif

// IDropTarget implementation

Windnd::Windnd(
    HWND hgwnd,
    Move_shape_handler_fun movefun,
    Move_combo_handler_fun movecmbfun,
    Drop_shape_handler_fun shapefun,
    Drop_chunk_handler_fun cfun,
    Drop_npc_handler_fun npcfun,
    Drop_combo_handler_fun combfun
) : gamewin(hgwnd), move_shape_handler(movefun),
	move_combo_handler(movecmbfun), shape_handler(shapefun),
	chunk_handler(cfun), npc_handler(npcfun),
	combo_handler(combfun), drag_id(-1) {
	std::memset(&data, 0, sizeof(data));
	m_cRef = 1;
}

STDMETHODIMP DECLSPEC_NOTHROW
Windnd::QueryInterface(REFIID iid, void **ppvObject) {
	*ppvObject = nullptr;
	if (IID_IUnknown == iid || IID_IDropTarget == iid)
		*ppvObject = this;
	if (nullptr == *ppvObject)
		return E_NOINTERFACE;
	static_cast<LPUNKNOWN>(*ppvObject)->AddRef();
	return NOERROR;
}

STDMETHODIMP_(ULONG) DECLSPEC_NOTHROW
Windnd::AddRef() {
	return ++m_cRef;
}

STDMETHODIMP_(ULONG) DECLSPEC_NOTHROW
Windnd::Release() {
	if (0 != --m_cRef)
		return m_cRef;
	delete this;
	return 0;
}

STDMETHODIMP DECLSPEC_NOTHROW
Windnd::DragEnter(IDataObject *pDataObject,
                  DWORD grfKeyState,
                  POINTL pt,
                  DWORD *pdwEffect) {
	ignore_unused_variable_warning(grfKeyState, pt);
	if (!is_valid(pDataObject)) {
		*pdwEffect = DROPEFFECT_NONE;
	} else {
		*pdwEffect = DROPEFFECT_COPY;
	}

	std::memset(&data, 0, sizeof(data));

	FORMATETC fetc;
	fetc.cfFormat = CF_EXULT;
	fetc.ptd = nullptr;
	fetc.dwAspect = DVASPECT_CONTENT;
	fetc.lindex = -1;
	fetc.tymed = TYMED_HGLOBAL;

	STGMEDIUM med;
	pDataObject->GetData(&fetc, &med);
	const unsigned char *dragdata = static_cast<unsigned char *>(GlobalLock(med.hGlobal));

	if (Is_u7_shapeid(dragdata)) {
		drag_id = U7_TARGET_SHAPEID;
		Get_u7_shapeid(dragdata, data.shape.file, data.shape.shape, data.shape.frame);
	} else if (Is_u7_chunkid(dragdata)) {
		drag_id = U7_TARGET_CHUNKID;
		Get_u7_chunkid(dragdata, data.chunk.chunknum);
	} else if (Is_u7_comboid(dragdata)) {
		drag_id = U7_TARGET_COMBOID;
		Get_u7_comboid(dragdata, data.combo.xtiles, data.combo.ytiles, data.combo.right, data.combo.below, data.combo.cnt, data.combo.combo);
	} else if (Is_u7_npcid(dragdata)) {
		drag_id = U7_TARGET_NPCID;
		Get_u7_npcid(dragdata, data.npc.npcnum);
	}

	GlobalUnlock(med.hGlobal);
	ReleaseStgMedium(&med);

	prevx = -1;
	prevy = -1;

	return S_OK;
}

STDMETHODIMP DECLSPEC_NOTHROW
Windnd::DragOver(DWORD grfKeyState,
                 POINTL pt,
                 DWORD *pdwEffect) {
	ignore_unused_variable_warning(grfKeyState);
	*pdwEffect = DROPEFFECT_COPY;
	// Todo

	POINT pnt = { pt.x, pt.y};
	ScreenToClient(gamewin, &pnt);

	switch (drag_id) {

	case U7_TARGET_SHAPEID:
		if (data.shape.file == U7_SHAPE_SHAPES) {
			if (move_shape_handler) move_shape_handler(data.shape.shape, data.shape.frame,
				        pnt.x, pnt.y, prevx, prevy, true);
		}
		break;

	case U7_TARGET_COMBOID:
		if (data.combo.cnt > 0) {
			if (move_combo_handler) move_combo_handler(data.combo.xtiles, data.combo.ytiles,
				        data.combo.right, data.combo.below, pnt.x, pnt.y, prevx, prevy, true);
		}
		break;

	default:
		break;
	}

	prevx = pnt.x;
	prevy = pnt.y;

	return S_OK;
}

STDMETHODIMP DECLSPEC_NOTHROW
Windnd::DragLeave() {
	if (move_shape_handler)
		move_shape_handler(-1, -1, 0, 0, prevx, prevy, true);

	switch (drag_id) {
	case U7_TARGET_SHAPEID:
		break;
	case U7_TARGET_COMBOID:
		delete data.combo.combo;
		break;

	default:
		break;
	}
	std::memset(&data, 0, sizeof(data));
	drag_id = -1;

	return S_OK;
}

STDMETHODIMP DECLSPEC_NOTHROW
Windnd::Drop(IDataObject *pDataObject,
             DWORD grfKeyState,
             POINTL pt,
             DWORD *pdwEffect) {
	ignore_unused_variable_warning(grfKeyState);
	*pdwEffect = DROPEFFECT_COPY;

	// retrieve the dragged data
	FORMATETC fetc;
	fetc.cfFormat = CF_EXULT;
	fetc.ptd = nullptr;
	fetc.dwAspect = DVASPECT_CONTENT;
	fetc.lindex = -1;
	fetc.tymed = TYMED_HGLOBAL;

	STGMEDIUM med;
	pDataObject->GetData(&fetc, &med);
	const unsigned char *dragdata = static_cast<unsigned char *>(GlobalLock(med.hGlobal));

	POINT pnt = { pt.x, pt.y};
	ScreenToClient(gamewin, &pnt);

	if (Is_u7_shapeid(dragdata)) {
		int file;
		int shape;
		int frame;
		Get_u7_shapeid(dragdata, file, shape, frame);
		std::cerr << "(Exult) Dropping SHAPE at " << pt.x << " " << pt.y << " for " << file << " " << shape << " " << frame << std::endl;
		if (file == U7_SHAPE_SHAPES) {
			if (shape_handler) (*shape_handler)(shape, frame, pnt.x, pnt.y);
		}
	} else if (Is_u7_npcid(dragdata)) {
		int npcnum;
		Get_u7_npcid(dragdata, npcnum);
		std::cerr << "(Exult) Dropping NPC at " << pt.x << " " << pt.y << " for " << npcnum << std::endl;
		if (npc_handler) (*npc_handler)(npcnum, pnt.x, pnt.y);
	} else if (Is_u7_chunkid(dragdata)) {
		int chunknum;
		Get_u7_chunkid(dragdata, chunknum);
		std::cerr << "(Exult) Dropping CHUNK at " << pt.x << " " << pt.y << " for " << chunknum << std::endl;
		if (chunk_handler) (*chunk_handler)(chunknum, pnt.x, pnt.y);
	} else if (Is_u7_comboid(dragdata)) {
		int xtiles;
		int ytiles;
		int right;
		int below;
		int cnt;
		U7_combo_data *combo;
		Get_u7_comboid(dragdata, xtiles, ytiles, right, below, cnt, combo);
		std::cerr << "(Exult) Dropping COMBO at " << pt.x << " " << pt.y << " for " << xtiles << " " << ytiles << " " << right << " " << below << " " << cnt << std::endl;
		if (combo_handler) (*combo_handler)(cnt, combo, pnt.x, pnt.y);
		delete combo;
	}

	GlobalUnlock(med.hGlobal);
	ReleaseStgMedium(&med);

	return S_OK;
}

bool Windnd::is_valid(IDataObject *pDataObject) {
	FORMATETC fetc;
	fetc.cfFormat = CF_EXULT;
	fetc.ptd = nullptr;
	fetc.dwAspect = DVASPECT_CONTENT;
	fetc.lindex = -1;
	fetc.tymed = TYMED_HGLOBAL;

	return SUCCEEDED(pDataObject->QueryGetData(&fetc));
}

#endif
