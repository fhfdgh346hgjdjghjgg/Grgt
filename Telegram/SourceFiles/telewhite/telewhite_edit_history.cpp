/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_edit_history.h"

#include "history/history.h"
#include "history/history_item.h"
#include "history/history_item_components.h"
#include "lang/lang_keys.h"
#include "main/main_session.h"
#include "telewhite/telewhite_mods.h"
#include "ui/layers/generic_box.h"
#include "ui/vertical_list.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/popup_menu.h"
#include "ui/wrap/vertical_layout.h"
#include "window/window_session_controller.h"
#include "styles/style_layers.h"
#include "styles/style_menu_icons.h"

namespace Telewhite {
namespace {

constexpr auto kMaxVersions = 50;

using VersionsKey = std::pair<uint64, FullMsgId>;
using VersionsMap = base::flat_map<VersionsKey, std::vector<EditVersion>>;

[[nodiscard]] VersionsMap &Versions() {
	static auto result = VersionsMap();
	return result;
}

[[nodiscard]] VersionsKey Key(not_null<HistoryItem*> item) {
	return { item->history()->session().uniqueId(), item->fullId() };
}

[[nodiscard]] EditVersion CurrentVersion(not_null<HistoryItem*> item) {
	const auto edited = item->Get<HistoryMessageEdited>();
	return {
		.text = item->originalText(),
		.date = edited ? edited->date : item->date(),
	};
}

void EditHistoryBox(
		not_null<Ui::GenericBox*> box,
		std::vector<EditVersion> versions) {
	box->setTitle(tr::lng_telewhite_edit_history());
	box->setWidth(st::boxWideWidth);

	const auto container = box->verticalLayout();
	const auto count = int(versions.size());
	for (auto i = count; i != 0;) {
		auto &version = versions[--i];
		const auto date = FormatExactDateTime(version.date);
		const auto title = (i + 1 == count)
			? tr::lng_telewhite_edit_history_current(tr::now, lt_date, date)
			: date;
		Ui::AddSubsectionTitle(container, rpl::single(title));
		if (version.text.text.isEmpty()) {
			version.text = TextWithEntities{
				tr::lng_telewhite_edit_history_empty(tr::now),
			};
		}
		const auto label = container->add(
			object_ptr<Ui::FlatLabel>(
				container,
				rpl::single(std::move(version.text)),
				st::boxLabel),
			st::boxRowPadding);
		label->setSelectable(true);
		Ui::AddSkip(container);
	}

	box->addButton(tr::lng_close(), [=] { box->closeBox(); });
}

} // namespace

std::optional<EditVersion> EditSnapshot(not_null<HistoryItem*> item) {
	if (!Enabled(Mod::EditHistory) || !item->isRegular()) {
		return std::nullopt;
	}
	return CurrentVersion(item);
}

void RememberEdit(
		not_null<HistoryItem*> item,
		std::optional<EditVersion> was) {
	if (!was || was->text == item->originalText()) {
		return;
	}
	auto &list = Versions()[Key(item)];
	list.push_back(std::move(*was));
	if (int(list.size()) > kMaxVersions) {
		list.erase(begin(list));
	}
}

void AddEditHistoryAction(
		not_null<Ui::PopupMenu*> menu,
		not_null<Window::SessionController*> controller,
		not_null<HistoryItem*> item) {
	const auto i = Versions().find(Key(item));
	if (i == end(Versions()) || i->second.empty()) {
		return;
	}
	auto versions = i->second;
	versions.push_back(CurrentVersion(item));
	menu->addAction(tr::lng_telewhite_edit_history(tr::now), [=] {
		controller->show(Box(EditHistoryBox, versions));
	}, &st::menuIconEdit);
}

} // namespace Telewhite
