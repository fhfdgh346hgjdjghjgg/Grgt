/*
This file is part of Telewhite, a fork of Telegram Desktop.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "telewhite/telewhite_peer_info.h"

#include "apiwrap.h"
#include "data/data_channel.h"
#include "data/data_chat.h"
#include "data/data_peer.h"
#include "main/main_session.h"
#include "telewhite/telewhite_mods.h"

namespace Telewhite {
namespace {

[[nodiscard]] TextWithEntities DateText(TimeId date) {
	return TextWithEntities{ date ? FormatExactDateTime(date) : QString() };
}

// Message #1 of a channel is the "channel created" service message.
[[nodiscard]] TimeId FirstMessageDate(const MTPmessages_Messages &result) {
	return result.match([](const MTPDmessages_messagesNotModified &) {
		return TimeId();
	}, [](const auto &data) {
		for (const auto &message : data.vmessages().v) {
			const auto date = message.match([](
					const MTPDmessageService &data) {
				return (data.vid().v == 1) ? TimeId(data.vdate().v) : TimeId();
			}, [](const auto &) {
				return TimeId();
			});
			if (date) {
				return date;
			}
		}
		return TimeId();
	});
}

} // namespace

rpl::producer<TextWithEntities> CreationDateValue(not_null<PeerData*> peer) {
	if (const auto chat = peer->asChat()) {
		return rpl::single(DateText(chat->date));
	}
	const auto channel = peer->asChannel();
	if (!channel) {
		return rpl::single(TextWithEntities());
	}
	if (const auto from = channel->getMigrateFromChat(); from && from->date) {
		return rpl::single(DateText(from->date));
	} else if (!channel->amIn() && channel->date) {
		// For non-members the server sends the creation date here.
		return rpl::single(DateText(channel->date));
	}
	return [=](auto consumer) {
		auto lifetime = rpl::lifetime();
		const auto api = &channel->session().api();
		const auto requestId = api->request(MTPchannels_GetMessages(
			channel->inputChannel(),
			MTP_vector<MTPInputMessage>(1, MTP_inputMessageID(MTP_int(1)))
		)).done([=](const MTPmessages_Messages &result) {
			consumer.put_next(DateText(FirstMessageDate(result)));
		}).send();
		lifetime.add([=] {
			api->request(requestId).cancel();
		});
		return lifetime;
	};
}

} // namespace Telewhite
