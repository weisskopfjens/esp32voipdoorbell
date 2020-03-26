/*
 * In The Name Of God
 * ========================================
 * [] File Name : RTPPacket.cpp
 *
 * [] Creation Date : 05-06-2017
 *
 * [] Created By : Parham Alvani (parham.alvani@gmail.com)
 * =======================================
*/
/*
 * Copyright (c) 2017 Parham Alvani.
*/


#include "RTPPacket.h"

RTPPacket::RTPPacket(const uint8_t *payload, uint16_t sequenceNumber, uint32_t ssrc, uint32_t timestamp, uint8_t payloadType, int payloadLength){
	this->payload = payload;
	this->sequenceNumber = sequenceNumber;
	this->ssrc = ssrc;
	this->timestamp = timestamp;
	this->payloadType = payloadType;
    this->payloadLength = payloadLength;
}

int RTPPacket::serialize(uint8_t *buff) const
{
	/* buff[0] = (V << 6 | P << 5 | X << 4 | CC) */
	buff[0] = (2 << 6 | 0 << 5 | 0 << 4 | 0);
	/* buff[1] = (M << 7 | PT) */
	buff[1] = (0 << 7 | this->payloadType);
	/* buff[2, 3] = SN */
	buff[2] = this->sequenceNumber >> 8;
	buff[3] = this->sequenceNumber;
	/* buff[4, 5, 6, 7] = TS */
	buff[4] = this->timestamp >> 24;
	buff[5] = this->timestamp >> 16;
	buff[6] = this->timestamp >> 8;
	buff[7] = this->timestamp;
	/* buff[8, 9, 10, 11] = SSRC */
	buff[8] = this->ssrc >> 24;
	buff[9] = this->ssrc >> 16;
	buff[10] = this->ssrc >> 8;
	buff[11] = this->ssrc;

	int i = this->payloadLength;
    if (i == 0) {
	    while (this->payload[i] != 0) {
		    buff[12 + i] = this->payload[i];
		    i++;
	    }
	    buff[12 + i] = 0;
    } else {
        int j;
        for (j = 0; j < i; j++) {
            buff[12 + j] = this->payload[j];
        }
        buff[12 + j] = 0;
    }

	return i + 12;
}

int RTPPacket::deserialize(const uint8_t *buff, int length)
{
	/* buff[0] = (V << 6 | P << 5 | X << 4 | CC) */
	if ((buff[0] & 0xC0) >> 6 != 2) {
		return 0;
	}
	/* buff[1] = (M << 7 | PT) */
	this->payloadType = (buff[1] & 0x7F);
	/* buff[2, 3] = SN */
	this->sequenceNumber = buff[2] << 8 | buff[3];
	/* buff[4, 5, 6, 7] = TS */
	this->timestamp = buff[4] << 24 | buff[5] << 16 | buff[6] << 8 | buff[7];
	/* buff[8, 9, 10, 11] = SSRC */
	this->ssrc = buff[8] << 24 | buff[9] << 16 | buff[10] << 8 | buff[11];
	this->payloadLength = length - 12; // 12byte header
	this->payload = buff+12;
	return length-12;
}

const uint8_t *RTPPacket::getPayload() const
{
	return this->payload;
}
