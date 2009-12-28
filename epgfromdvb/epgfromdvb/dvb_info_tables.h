/* 
* tv_grab_dvb - (c) Mark Bryars 2004
* God bless vim and macros, this would have taken forever to format otherwise.
* 
*/

#include "lookup.h"

static struct lookup_table main_description_table[]={ 
	{ 0x01, 0, { "Movie", "�����", NULL, NULL }}, 
	{ 0x02, 0, { "News", "����� ��������", NULL, NULL }}, 
	{ 0x03, 1, { "Show", "����� ������", NULL, NULL }}, 
	{ 0x04, 0, { "Sports", "�����", NULL, NULL }}, 
	{ 0x05, 1, { "Children's/Youth", "����� �����", NULL, NULL }}, 
	{ 0x06, 0, { "Music", "������", NULL, NULL }}, 
	{ 0x07, 0, { "Arts/Culture", "����� �������", NULL, NULL }}, 
	{ 0x08, 0, { "Politics/Economy", "����� �������", NULL, NULL }}, 
	{ 0x09, 0, { "Science/Documentary", "�����, ��� ����", NULL, NULL }}, 
	{ 0x0a, 1, { "Leisure/Lifestyle", "���� ������� ����", NULL, NULL }}, 
	{ 0x0b, 1, { "Series", "�����", NULL, NULL }}, 
	{ 0x0c, 0, { "Shopping", "�����", NULL, NULL }}, 
	{ 0x0d, 0, { "Adult", "�������", NULL, NULL }}, 
	{ 0x0f, 0, { "Adult", "�������", NULL, NULL }}, 
	{ -1, 0, { NULL, NULL, NULL, NULL } }    
}; 

static struct lookup_table description_table[]={ 
	{ 0x10, 0, { "Short Movie", "�����", NULL, NULL }}, 
	{ 0x11, 0, { "Detective/Thriller",	"����", NULL, NULL }}, 
	{ 0x12, 0, { "Adventure/Western/War", "��������", NULL, NULL }}, 
	{ 0x13, 0, { "Science Fiction/Fantasy/Horror", "��� ������, ������" }}, 
	{ 0x14, 0, { "Comedy", "������", NULL, NULL  }}, 
	{ 0x15, 0, { "Soap/Melodrama/Folkloric", "����", NULL, NULL  }}, 
	{ 0x16, 0, { "Romance", "�����", NULL, NULL  }}, 
	{ 0x17, 0, { "Western", "������", NULL, NULL }},
	{ 0x18, 0, { "Adult Movie/Drama", "�������", NULL, NULL  }}, 
	{ 0x19, 0, { "Drama", "����", NULL, NULL  }}, 
	{ 0x1a, 0, { "Thriller", "���", NULL, NULL  }}, 
	{ 0x1d, 0, { "War", "�����", NULL, NULL  }}, 
	{ 0x1f, 0, { "Musicle", "�������", NULL, NULL  }}, 
	{ 0x1C, 0, { "Romantic Comedy", "������ �������", NULL,NULL }}, 
	{ 0x1E, 0, { "Historical Drama", "���� �������", NULL,NULL }}, 
	{ 0x1B, 0, { "Crime", "���", NULL,NULL }}, 
	{ 0x20, 0, { "News/Current Affairs (General)", "����� �������� ����", NULL, NULL  }}, 
	{ 0x21, 0, { "News/Weather report", "News/Weather report", NULL, NULL }},
	{ 0x22, 0, { "News Magazine", "�����", NULL, NULL  }}, 
	{ 0x23, 0, { "Documentary", "����", NULL, NULL  }}, 
	{ 0x24, 0, { "Discussion/Interview/Debate", "Discussion/Interview/Debate", NULL, NULL }}, 
	{ 0x26, 0, { "News Flash", "������ �����", NULL, NULL  }}, 
	{ 0x27, 0, { "Journalist Report", "�����", NULL, NULL  }}, 
	{ 0x28, 0, { "Religion", "�� ������", NULL, NULL  }}, 
	{ 0x2f, 0, { "Special", "�����", NULL, NULL  }}, 
	{ 0x30, 0, { "Show/Game Show (General)", "����� ������", NULL, NULL  }}, 
	{ 0x31, 0, { "Game Show/Quiz/Contest", "�������", NULL, NULL  }}, 
	{ 0x32, 0, { "Variety Show", "Variety Show", NULL, NULL }},
	{ 0x33, 0, { "Talk Show", "������", NULL, NULL  }}, 
	{ 0x34, 0, { "Playet", "��������", NULL, NULL  }}, 
	{ 0x35, 0, { "Talk Show", "����� �����, ��� ����", NULL, NULL  }}, 
	{ 0x36, 0, { "Light Night", "���� ����", NULL, NULL  }}, 
	{ 0x37, 0, { "Special", "������", NULL, NULL  }}, 
	{ 0x3f, 0, { "Magazine", "�����", NULL, NULL  }},
	{ 0x38, 0, { "Morning Program", "������� ����", NULL, NULL }}, 
	{ 0x40, 0, { "General", "����", NULL, NULL  }}, 
	{ 0x41, 0, { "Special Event", "Special Event", NULL, NULL  }}, 
	{ 0x42, 0, { "Sports Magazine", "����� �����", NULL, NULL  }}, 
	{ 0x43, 0, { "Football/Soccer", "������", NULL, NULL  }}, 
	{ 0x44, 0, { "Tennis/Squash", "����", NULL, NULL  }}, 
	{ 0x45, 0, { "Basketball", "������", NULL, NULL }}, 
	{ 0x46, 0, { "Ahtletics", "Ahtletics", NULL, NULL  }}, 
	{ 0x47, 0, { "Motor Sport", "����� ������", NULL, NULL  }}, 
	{ 0x48, 0, { "Water Sport", "����� ���, �����", NULL, NULL  }}, 
	{ 0x49, 0, { "Winter Sports", "Winter Sports", NULL, NULL  }}, 
	{ 0x4a, 0, { "Equestrian", "����� �����", NULL, NULL  }}, 
	{ 0x4b, 0, { "Martial Sports", "Martial Sports", NULL, NULL  }}, 
	{ 0x4c, 0, { "Cycling", "�������", NULL, NULL  }}, 
	{ 0x4d, 0, { "Gymnastics/Aerobics", "�������, ������", NULL, NULL  }}, 
	{ 0x4e, 0, { "Ball Games", "����� ����", NULL, NULL  }}, 
	{ 0x4f, 0, { "Extreme Sports", "����� �����", NULL, NULL  }}, 
	{ 0x50, 0, { "Children's/Youth (General)", "����� �����, ����", NULL, NULL  }}, 
	{ 0x51, 0, { "Pre-school", "���� ���, ���� ���", NULL, NULL  }}, 
	{ 0x52, 0, { "6 to 14", "�����", NULL, NULL  }}, 
	{ 0x53, 0, { "10 to 16", "10 to 16", NULL, NULL  }}, 
	{ 0x54, 0, { "Informational/Educational/School", "����� �������", NULL, NULL  }}, 
	{ 0x55, 0, { "Cartoons/Puppets", "���� �������", NULL, NULL  }},						// Changed by Michael on 6/10/06
	{ 0x59, 1, { "Serial", "���� ����", NULL, NULL  }}, 
	{ 0x5b, 0, { "Puppets", "�����", NULL, NULL  }}, 
	{ 0x5c, 0, { "Adventure", "��������", NULL, NULL  }}, 
	{ 0x5A, 1, { "Animation Series", "���� �������", NULL, NULL }}, 
	{ 0x60, 0, { "Music/Ballet/Dance (General)", "����", NULL, NULL  }}, 
	{ 0x61, 0, { "Rock/Pop", "Rock/Pop", NULL, NULL  }}, 
	{ 0x62, 0, { "Classical Music", "������ ������", NULL, NULL  }}, 
	{ 0x63, 0, { "Folk/Traditional Music", "Folk/Traditional Music", NULL, NULL  }}, 
	{ 0x64, 0, { "Jazz", "Jazz", NULL, NULL  }}, 
	{ 0x65, 0, { "Musical/Opera", "Musical/Opera", NULL, NULL  }}, 
	{ 0x66, 0, { "Ballet", "Ballet", NULL, NULL  }}, 
	{ 0x70, 0, { "Arts/Culture (without Music, General)", "����� ������", NULL, NULL  }}, 
	{ 0x71, 0, { "Performing Arts", "Performing Arts", NULL, NULL  }}, 
	{ 0x72, 0, { "Fine Arts", "Fine Arts", NULL, NULL  }}, 
	{ 0x73, 0, { "Religion", "�������", NULL, NULL  }}, 
	{ 0x74, 0, { "Popular Culture/Traditional Arts", "Popular Culture/Traditional Arts", NULL, NULL  }}, 
	{ 0x75, 0, { "Literature", "�����", NULL, NULL  }}, 
	{ 0x76, 0, { "Film/Cinema", "Film/Cinema", NULL, NULL  }}, 
	{ 0x77, 0, { "Experimental Film/Video", "Experimental Film/Video", NULL, NULL  }}, 
	{ 0x78, 0, { "Broadcasting/Press", "Broadcasting/Press", NULL, NULL  }}, 
	{ 0x79, 0, { "New Media", "New Media", NULL, NULL  }}, 
	{ 0x7a, 0, { "Arts/Culture Magazines", "�����", NULL, NULL  }}, 
	{ 0x7b, 0, { "Fashion", "Fashion", NULL, NULL  }}, 
	{ 0x7e, 0, { "Festival", "������", NULL, NULL  }}, 
	{ 0x80, 0, { "Social/Political Issues/Economics (General)", "����� �������", NULL, NULL  }}, 
	{ 0x81, 0, { "Magazine/Report/Documentary", "Magazine/Report/Documentary", NULL, NULL  }}, 
	{ 0x82, 0, { "Economics/Social Advisory", "������, �������", NULL, NULL  }}, 
	{ 0x83, 0, { "Remarkable People", "�����", NULL, NULL  }}, 
	{ 0x85, 0, { "Economy", "�����", NULL, NULL  }}, 
	{ 0x86, 0, { "Technology/Innovation", "��������� ��������", NULL, NULL  }}, 
	{ 0x90, 0, { "Education/Science/Factual Topics (General)", "Education/Science/Factual Topics (General)", NULL, NULL  }}, 
	{ 0x91, 0, { "Nature/Animals/Environment", "���, ���� ����", NULL, NULL  }}, 
	{ 0x92, 0, { "Technology/Natural Sciences", "Technology/Natural Sciences", NULL, NULL  }}, 
	{ 0x93, 0, { "Medicine/Physiology/Psychology", "Medicine/Physiology/Psychology", NULL, NULL  }}, 
	{ 0x94, 0, { "Foreign Countries/Expeditions", "���������, ��������", NULL, NULL  }}, 
	{ 0x95, 0, { "Social/Spiritual Sciences", "Social/Spiritual Sciences", NULL, NULL  }}, 
	{ 0x96, 0, { "Further Education", "Further Education", NULL, NULL  }}, 
	{ 0x97, 0, { "Languages", "������, ������", NULL, NULL  }}, 
	{ 0x98, 0, { "Documentary", "����������", NULL, NULL  }}, 
	{ 0x9a, 0, { "Languages/Literature", "����, �����", NULL, NULL  }}, 
	{ 0x9f, 0, { "History/Archeology", "�������� ������������", NULL, NULL  }}, 
	{ 0xa0, 0, { "Leisure Hobbies (General)", "Leisure Hobbies (General)", NULL, NULL  }}, 
	{ 0xa1, 0, { "Tourism/Travel", "���� ������ ����", NULL, NULL  }}, 
	{ 0xa2, 0, { "Handicraft", "Handicraft", NULL, NULL  }}, 
	{ 0xa3, 0, { "Motoring", "���� ��������", NULL, NULL  }}, 
	{ 0xa4, 0, { "Fitness and Health", "�����", NULL, NULL  }}, 
	{ 0xa5, 0, { "Cooking", "�����", NULL, NULL  }}, 
	{ 0xa6, 0, { "Advertisement/Shopping", "������", NULL, NULL  }}, 
	{ 0xa7, 0, { "Gardening", "Gardening", NULL, NULL  }}, 
	{ 0xa9, 0, { "Fashion", "�����", NULL, NULL  }}, 
	{ 0xaa, 1, { "Health", "������", NULL, NULL }},
	{ 0xab, 0, { "Furniture/Handicraft", "����� �������", NULL, NULL  }}, 
	{ 0xac, 0, { "Advisory", "�����", NULL, NULL  }}, 
	{ 0xad, 0, { "Shopping", "�����", NULL, NULL  }}, 
	{ 0xb0, 1, { "Serial", "�����", NULL, NULL  }}, 
	{ 0xb1, 1, { "Soap Opera/Telenovella", "����� ����, �������", NULL, NULL  }}, 
	{ 0xb2, 1, { "Comedy", "������", NULL, NULL  }}, 
	{ 0xb3, 1, { "Family", "�����", NULL, NULL  }}, 
	{ 0xb4, 1, { "Mini-Serial", "���� �����", NULL, NULL  }}, 
	{ 0xb6, 1, { "Police", "�����", NULL, NULL }},
	{ 0xb7, 1, { "Animation", "�������", NULL, NULL  }}, 
	{ 0xb9, 1, { "Drama", "����", NULL, NULL  }}, 
	{ 0xBA, 1, { "Relality", "�������", NULL, NULL }}, 
	{ 0xbb, 1, { "Adventure", "��������", NULL, NULL  }}, 
	{ 0xBE, 1, { "Comical Drama", "���� �����", NULL,NULL }},
	{ 0xBF, 1, { "Horror", "����", NULL, NULL }}, 
	{ 0xf5, 0, { "Adult", "�������", NULL, NULL }}, 
	{ -1, 0, {NULL, NULL, NULL, NULL  }}    
}; 

static struct lookup_table aspect_table[] = {
	{0x00, 0, {NULL, NULL, NULL, NULL}},		// reserved
	{0x01, 0, {"4:3", NULL, NULL, NULL}},		// MPEG-2 video, 4:3 aspect ratio, 25 Hz
	{0x02, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 video, 16:9 aspect ratio with pan vectors, 25 Hz
	{0x03, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 video, 16:9 aspect ratio without pan vectors, 25 Hz
	{0x04, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 video, > 16:9 aspect ratio, 25 Hz
	{0x05, 0, {"4:3", NULL, NULL, NULL}},		// MPEG-2 video, 4:3 aspect ratio, 30 Hz
	{0x06, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 video, 16:9 aspect ratio with pan vectors, 30 Hz
	{0x07, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 video, 16:9 aspect ratio without pan vectors, 30 Hz
	{0x08, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 video, > 16:9 aspect ratio, 30 Hz
	{0x09, 0, {"4:3", NULL, NULL, NULL}},		// MPEG-2 high definition video, 4:3 aspect ratio, 25 Hz
	{0x0A, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 high definition video, 16:9 aspect ratio with pan vectors, 25 Hz
	{0x0B, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 high definition video, 16:9 aspect ratio without pan vectors, 25 Hz
	{0x0C, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 high definition video, > 16:9 aspect ratio, 25 Hz
	{0x0D, 0, {"4:3", NULL, NULL, NULL}},		// MPEG-2 high definition video, 4:3 aspect ratio, 30 Hz
	{0x0E, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 high definition video, 16:9 aspect ratio with pan vectors, 30 Hz
	{0x0F, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 high definition video, 16:9 aspect ratio without pan vectors, 30 Hz
	{0x10, 0, {"16:9", NULL, NULL, NULL}},		// MPEG-2 high definition video, > 16:9 aspect ratio, 30 Hz
	{-1, 0, {NULL, NULL, NULL, NULL }}	
};

static struct lookup_table audio_table[] = {
	{0x00, 0, {NULL, NULL, NULL, NULL}},		// reserved
	{0x01, 0, {"mono", NULL, NULL, NULL }},		// MPEG-1 Layer 2 audio, single mono channel
	{0x02, 0, {"mono", NULL, NULL, NULL }},	// MPEG-1 Layer 2 audio, dual mono channel
	{0x03, 0, {"stereo", NULL, NULL, NULL }},	// MPEG-1 Layer 2 audio, stereo (2 channel)
	{0x04, 0, {"stereo", NULL, NULL, NULL }},	// MPEG-1 Layer 2 audio, multi-lingual, multi-channel
	{0x05, 0, {"surround", NULL, NULL, NULL }},	// MPEG-1 Layer 2 audio, surround sound
	{0x40, 0, {"stereo", NULL, NULL, NULL}},	// MPEG-1 Layer 2 audio description for the visually impaired
	{0x41, 0, {"stereo", NULL, NULL, NULL}},	// MPEG-1 Layer 2 audio for the hard of hearing
	{0x42, 0, {"stereo", NULL, NULL, NULL}},	// receiver-mixed supplementary audio as per annex G of TR 101 154 [10]
	{-1, 0, {NULL, NULL, NULL, NULL}}
};
