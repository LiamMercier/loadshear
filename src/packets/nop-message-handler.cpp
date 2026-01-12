// Copyright (c) 2026 Liam Mercier
//
// This file is part of Loadshear.
//
// Loadshear is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License Version 3.0
// as published by the Free Software Foundation.
//
// Loadshear is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License v3.0
// for more details.
//
// You should have received a copy of the GNU General Public License v3.0
// along with Loadshear. If not, see <https://www.gnu.org/licenses/gpl-3.0.txt>

#include "nop-message-handler.h"

void NOPMessageHandler::parse_message(std::span<const uint8_t> header,
                                      std::span<const uint8_t> body,
                                      std::function<void(ResponsePacket)> callback)
                                                                              const
{
    static const std::shared_ptr<std::vector<uint8_t>> unused
        = std::make_shared<std::vector<uint8_t>>();

    callback({std::move(unused)});
}

HeaderResult NOPMessageHandler::parse_header(std::span<const uint8_t> buffer) const
{
    return {0, HeaderResult::Status::OK};
}
