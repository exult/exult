/*
Copyright (C) 2026 The Exult Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef MUI_OPTIONS_H
#define MUI_OPTIONS_H

#include "Mui_config_editor.h"
#include "Mui_options_model.h"
#include "Mui_window.h"

#include <memory>
#include <string>
#include <vector>

/*
 *  The themed Options window: one tab per classic option screen (Video,
 *  Audio, Engine, Display, Input, UI) plus the Advanced tab. Every page is
 *  driven by its classic gump through Mui_options_model, so the settings
 *  behave and are saved exactly as in the classic screens. A search field
 *  in the title bar finds settings on every page by name, help text or
 *  config key. Changes are held until Apply. The Advanced tab
 *  (Mui_config_editor) edits any exult.cfg key directly.
 */
class Mui_options_window : public Mui_window {
public:
	explicit Mui_options_window(const char* tab = nullptr);
	~Mui_options_window() override;

protected:
	void                     build(Mui_box& body) override;
	std::vector<std::string> tab_labels() const override;
	std::vector<std::string> tab_keys() const override;
	void                     on_tab(int index) override;
	void                     build_footer(Mui_box& footer) override;
	void                     build_title(Mui_box& bar) override;
	void                     on_cancel() override;
	std::string              default_help() const override;

private:
	using Model = Mui_options_model;
	using Row   = Mui_options_model::Row;

	std::vector<std::unique_ptr<Model>> models;    // One per classic tab.
	std::unique_ptr<Mui_config_editor>  editor;    // The Advanced tab.
	std::string                         search;
	std::string                         highlight;    // Control id of a found row.

	void create_models();
	void build_page(Mui_box& body, int tab);
	void build_row(Mui_box& body, int tab, const Row& r);
	void build_results(Mui_box& body);
	void build_advanced(Mui_box& body);
	bool matches(const Model& m, const Row& r, const std::string& needle) const;
	void change(int tab, const std::string& id, int index, bool defer_rebuild);
	void apply_all();
	void reset_page();
	void jump_to(int tab, const std::string& id);
	void jump_to_key(const std::string& path);
};

#endif
