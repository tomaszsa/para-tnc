/*
 * wx.c
 *
 *  Created on: 28.05.2017
 *      Author: mateusz
 */

#include "aprs/wx.h"
#include "aprs/digi.h"
#include "drivers/tx20.h"
#include "main.h"

#include "station_config.h"

void SendWXFrame(Anemometer* input, float temperatura, unsigned cisnienie) {
	float max_wind_speed = 0.0, temp;
	unsigned char wind_speed_mph, wind_gusts_mph, d;
	for(d = 1; d <= 15 ; d++)
		if (VNAME.HistoryAVG[d].WindSpeed > max_wind_speed)
				max_wind_speed = VNAME.HistoryAVG[d].WindSpeed * 0.1f;		// Wyszukiwane najwiekszej wartosci
	temp = input->HistoryAVG[0].WindSpeed;
	temp /= 0.45;																						// Konwersja na mile na godzine
	max_wind_speed /= 0.45;
	if ((temp - (short)temp) >= 0.5)												// Zaokraglanie wartosci
		/* Odejmuje od wartosci zmiennoprzecinkowej w milach nad godzine wartosc
			 po zrzutowaniu na typ short czyli z odcienta czescia po przecinku.
			 Jezeli wynik jest wiekszy albo rowny 0.5 to trzeba zaokraglic w gore */
		wind_speed_mph = (short)temp + 1;
	else
		/* A jezeli jest mniejsza niz 0.5 to zaokraglamy w dol */
		wind_speed_mph = (short)temp;
	if ((max_wind_speed - (short)max_wind_speed) >= 0.5)
		/* Analogiczna procedura ma miejsce dla porywow wiatru*/
		wind_gusts_mph = (short)max_wind_speed + 1;
	else
		wind_gusts_mph = (short)max_wind_speed;
	aprs_msg_len = sprintf(aprs_msg, "!%07.2f%c%c%08.2f%c%c%03d/%03dg%03dt%03dr...p...P...b%05d", _LAT, _LATNS, '/', _LON, _LONWE, '_', /* kierunek */(short)(input->HistoryAVG[0].WindDirX), /* predkosc*/(int)wind_speed_mph, /* porywy */(short)(wind_gusts_mph), /*temperatura */(short)(temperatura*1.8+32), cisnienie *10);
//	aprs_msg_len = sprintf(aprs_msg, "%s%03d/%03dg%03dt%03dr%03dp%03dP%03db%04d ~", "!5001.45N/02159.66E_", /* kierunek */90, /* predkosc*/(int)(2.1 * 2.23698), /* porywy */(short)(5.7 * 2.23698), /*temperatura */(short)(23 * 1.8 + 32),  0, 0, 0, 10130);
	aprs_msg[aprs_msg_len] = 0;
 	ax25_sendVia(&ax25, path, path_len, aprs_msg, aprs_msg_len);
	after_tx_lock = 1;
//		for(jj = 0; jj <= 0x19FFFF; jj++);
	while(ax25.dcd == true);
 	afsk_txStart(&a);
}


