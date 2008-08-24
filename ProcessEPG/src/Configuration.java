import java.util.*;
import java.io.*;

public class Configuration
{
	public String defaultLanguage;
	public int gmtOffset;
	public HashSet<Integer> forceRussian;
	public HashSet<Integer> forceEnglish;
	public HashSet<Integer> forceHebrew;
	public HashSet<Integer> forceArabic;
	public HashSet<Integer> excludeChannels;
	public boolean parseSeries;
	public HashMap<Integer, String> categorieMapping;
	public boolean createXMLTVImporterChannelsFile;
	private HashSet<Integer> channelsWithAllProgrammesAsSeries;
	
	public boolean shouldTreatAsSeries(int category, int channel)
	{
		// Everything aside of movies, sports and news is treated as series
		return parseSeries && (channelsWithAllProgrammesAsSeries.contains(channel) || ((category & 0xF0) != 0x10 && (category & 0xF0) != 0x40 && (category & 0xF0) != 0x20));
	}
	
	public String getLanguage(int channel)
	{
		if(forceRussian.contains(channel))
			return new String("ru");
		else if(forceEnglish.contains(channel))
			return new String("en");
		else if(forceHebrew.contains(channel))
			return new String("he");
		else if(forceArabic.contains(channel))
			return new String("ar");
		else
			return defaultLanguage;
	}
	
	private void parseForceLanguage(HashSet<Integer> forcedLanguage, String parameter)
	{
		String[] channels = parameter.split(",");
		for(int i = 0; i < channels.length; i++)
			forcedLanguage.add(Integer.parseInt(channels[i]));
	}
	
	public void init(String iniFileName)
	{
		try
		{
			BufferedReader iniFile = new BufferedReader(new InputStreamReader(new FileInputStream(new File(iniFileName))));
			for(String iniLine = iniFile.readLine(); iniLine != null && iniLine.length() > 0; iniLine = iniFile.readLine())
			{
				String[] tokens = iniLine.split("=");
				if(tokens.length != 2)
					continue;
				String directive = tokens[0].trim();
				String parameters = tokens[1].trim();
				if(directive.equalsIgnoreCase("GMTOffset"))
					gmtOffset = Integer.parseInt(parameters);
				else if(directive.equalsIgnoreCase("DefaultLanguage"))
					defaultLanguage = parameters;
				else if(directive.equalsIgnoreCase("ForceRussian"))
					parseForceLanguage(forceRussian, parameters);
				else if(directive.equalsIgnoreCase("ForceHebrew"))
					parseForceLanguage(forceHebrew, parameters);
				else if(directive.equalsIgnoreCase("ForceEnglish"))
					parseForceLanguage(forceEnglish, parameters);
				else if(directive.equalsIgnoreCase("ForceArabic"))
					parseForceLanguage(forceArabic, parameters);
				else if(directive.equalsIgnoreCase("ParseSeries"))
					parseSeries = (Integer.parseInt(parameters) == 1);
				else if(directive.equalsIgnoreCase("CreateXMLTVImporterChannelsFile"))
					createXMLTVImporterChannelsFile = (Integer.parseInt(parameters) == 1);
			}
		}
		catch (IOException e)
		{
		}
	}
	
	public Configuration()
	{
		createXMLTVImporterChannelsFile = false;
		parseSeries = true;
		defaultLanguage = new String("he");
		gmtOffset = 0;
		channelsWithAllProgrammesAsSeries = new HashSet<Integer>();
		channelsWithAllProgrammesAsSeries.add(12);
		channelsWithAllProgrammesAsSeries.add(13);
		channelsWithAllProgrammesAsSeries.add(14);
		channelsWithAllProgrammesAsSeries.add(15);
		channelsWithAllProgrammesAsSeries.add(501);
		forceRussian = new HashSet<Integer>();
		forceEnglish = new HashSet<Integer>();
		forceHebrew = new HashSet<Integer>();
		forceArabic = new HashSet<Integer>();
		/*forceRussian.add(9);
		forceRussian.add(181);
		forceRussian.add(182);
		forceRussian.add(183);
		forceRussian.add(184);
		forceRussian.add(185);
		forceRussian.add(186);
		forceRussian.add(187);
		forceRussian.add(188);
		forceRussian.add(189);*/
		
		categorieMapping = new HashMap<Integer, String>();
		categorieMapping.put(0x10, "סרטים - קצרים");
		categorieMapping.put(0x11, "סרטים - פעולה");
		categorieMapping.put(0x12, "סרטים - הרפתקאות");
		categorieMapping.put(0x13, "סרטים - מדע בדיוני, פנטזיה");
		categorieMapping.put(0x14, "סרטים - קומדיה");
		categorieMapping.put(0x15, "סרטים - אימה");
		categorieMapping.put(0x16, "סרטים - ילדים");
		categorieMapping.put(0x17, "סרטים - מערבון");
		categorieMapping.put(0x18, "סרטים - אנימציה");
		categorieMapping.put(0x19, "סרטים - דרמה");
		categorieMapping.put(0x1A, "סרטים - מתח");
		categorieMapping.put(0x1B, "סרטים - פשע");
		categorieMapping.put(0x1C, "סרטים - קומדיה רומנטית");
		categorieMapping.put(0x1D, "סרטים - מלחמה");
		categorieMapping.put(0x1E, "סרטים - דרמה תקופתית");
		categorieMapping.put(0x1F, "סרטים - מוסיקלי"); 
		categorieMapping.put(0x20, "חדשות ועינייני היום"); 
		categorieMapping.put(0x21, "חדשות - תחזית מזג האוויר");
		categorieMapping.put(0x22, "חדשות - מגזין"); 
		categorieMapping.put(0x23, "חדשות - כללי"); 
		categorieMapping.put(0x24, "חדשות - דיון מסביב לשולחן"); 
		categorieMapping.put(0x25, "חדשות - ראיון, דיון");
		categorieMapping.put(0x26, "מבזק חדשות"); 
		categorieMapping.put(0x27, "חדשות - תחקיר"); 
		categorieMapping.put(0x28, "חדשות - דת"); 
		categorieMapping.put(0x2f, "חדשות - מיוחד"); 
		categorieMapping.put(0x30, "בידור קל - כללי"); 
		categorieMapping.put(0x31, "בידור קל - שעשועון"); 
		categorieMapping.put(0x32, "בידור קל - וריאטה");
		categorieMapping.put(0x33, "בידור קל - סאטירה"); 
		categorieMapping.put(0x34, "בידור קל - מערכון"); 
		categorieMapping.put(0x35, "בידור קל - תכנית אירוח, טוק שואו"); 
		categorieMapping.put(0x36, "בידור קל - לייט נייט"); 
		categorieMapping.put(0x37, "בידור קל - ספיישל"); 
		categorieMapping.put(0x3d, "בידור קל - קניות");
		categorieMapping.put(0x3f, "בידור קל - מגזין");
		categorieMapping.put(0x38, "בידור קל - תוכניות בוקר"); 
		categorieMapping.put(0x39, "בידור קל - מוסיקה");
		categorieMapping.put(0x40, "ספורט - כללי"); 
		categorieMapping.put(0x41, "ספורט - אירוע מיוחד"); 
		categorieMapping.put(0x42, "ספורט - מגזין ספורט"); 
		categorieMapping.put(0x43, "ספורט - כדורגל"); 
		categorieMapping.put(0x44, "ספורט - טניס"); 
		categorieMapping.put(0x45, "ספורט - כדורסל"); 
		categorieMapping.put(0x46, "ספורט - אתלטיקה"); 
		categorieMapping.put(0x47, "ספורט - מוטורי"); 
		categorieMapping.put(0x48, "ספורט - מים"); 
		categorieMapping.put(0x49, "ספורט - חורף"); 
		categorieMapping.put(0x4a, "ספורט - רכיבה על סוסים");
		categorieMapping.put(0x4b, "ספורט - אומנויות לחימה");
		categorieMapping.put(0x4c, "ספורט - אופניים");
		categorieMapping.put(0x4d, "ספורט - התעמלות, אירובי");
		categorieMapping.put(0x4e, "ספורט - משחקי כדור");
		categorieMapping.put(0x4f, "ספורט - ספורט אתגרי");
		categorieMapping.put(0x50, "ילדים ונוער - כללי");
		categorieMapping.put(0x51, "ילדים ונוער - הגיל הרך, ילדי הגן");
		categorieMapping.put(0x52, "ילדים ונוער - 6 עד 14");
		categorieMapping.put(0x53, "ילדים ונוער - 10 עד 16");
		categorieMapping.put(0x54, "ילדים ונוער - תכנית לימודית");
		categorieMapping.put(0x55, "ילדים ונוער - סרטי אנימציה");
		categorieMapping.put(0x57, "ילדים ונוער - ערבית");
		categorieMapping.put(0x58, "ילדים ונוער - טריוויה");
		categorieMapping.put(0x59, "ילדים ונוער - סדרת דרמה");
		categorieMapping.put(0x5b, "ילדים ונוער - בובות");
		categorieMapping.put(0x5c, "ילדים ונוער - הרפתקאות");
		categorieMapping.put(0x5A, "ילדים ונוער - סדרת אנימציה");
		categorieMapping.put(0x5f, "ילדים ונוער - סדרה");
		categorieMapping.put(0x06, "מוסיקה - כללי");
		categorieMapping.put(0x60, "מוסיקה - כללי");
		categorieMapping.put(0x61, "מוסיקה - רוק, פופ");
		categorieMapping.put(0x62, "מוסיקה - קלאסית");
		categorieMapping.put(0x63, "מוסיקה - עממית, מסורתית");
		categorieMapping.put(0x64, "מוסיקה - ג'ז");
		categorieMapping.put(0x65, "מוסיקה - מיוזיקל, אופרה");
		categorieMapping.put(0x66, "מוסיקה - בלט");
		categorieMapping.put(0x68, "מוסיקה - עולם");
		categorieMapping.put(0x69, "מוסיקה - קליפים");
		categorieMapping.put(0x6c, "מוסיקה - אירוע");
		categorieMapping.put(0x6e, "מוסיקה - אולםנים");
		categorieMapping.put(0x70, "תרבות ואומנות - כללי");
		categorieMapping.put(0x71, "תרבות ואומנות - משחק");
		categorieMapping.put(0x72, "תרבות ואומנות - אומנות");
		categorieMapping.put(0x73, "תרבות ואומנות - תיאטרון");
		categorieMapping.put(0x74, "תרבות ואומנות - עממי, מסורתי");
		categorieMapping.put(0x75, "תרבות ואומנות - ספרות");
		categorieMapping.put(0x76, "תרבות ואומנות - קולנוע");
		categorieMapping.put(0x77, "תרבות ואומנות - קולנוע נסיוני");
		categorieMapping.put(0x78, "תרבות ואומנות - שידור, דפוס");
		categorieMapping.put(0x79, "תרבות ואומנות - מדיה חדשה");
		categorieMapping.put(0x7a, "תרבות ואומנות - מגזינים");
		categorieMapping.put(0x7b, "תרבות ואומנות - אופנה");
		categorieMapping.put(0x7c, "תרבות ואומנות - מופע");
		categorieMapping.put(0x7d, "תרבות ואומנות - קונצרט");
		categorieMapping.put(0x7e, "תרבות ואומנות - פסטיבל");
		categorieMapping.put(0x80, "פוליטיקה וכלכלה - כללי");
		categorieMapping.put(0x81, "פוליטיקה וכלכלה - מגזין");
		categorieMapping.put(0x82, "פוליטיקה וכלכלה - ייעוץ כלכלי");
		categorieMapping.put(0x83, "פוליטיקה וכלכלה - עסקים");
		categorieMapping.put(0x84, "פוליטיקה וכלכלה - כסף");
		categorieMapping.put(0x85, "פוליטיקה וכלכלה - כלכלה");
		categorieMapping.put(0x86, "פוליטיקה וכלכלה - טכנולוגיה וחדשנות");
		categorieMapping.put(0x90, "תעודה, טבע ומדע - כללי");
		categorieMapping.put(0x91, "תעודה, טבע ומדע - טבע, בעלי חיים");
		categorieMapping.put(0x92, "תעודה, טבע ומדע - מדעי טבע וטכנולוגיה");
		categorieMapping.put(0x93, "תעודה, טבע ומדע - רפואה, פיזיולוגיה ופסיכולוגיה");
		categorieMapping.put(0x94, "תעודה, טבע ומדע - מסעות לארצות אקזוטיות");
		categorieMapping.put(0x95, "תעודה, טבע ומדע - מדעי הרוח");
		categorieMapping.put(0x96, "תעודה, טבע ומדע - השכלה מתקדמת");
		categorieMapping.put(0x97, "תעודה, טבע ומדע - אומנויות וקולנוע");
		categorieMapping.put(0x98, "תעודה, טבע ומדע - תעודה");
		categorieMapping.put(0x99, "תעודה, טבע ומדע - תעודה");
		categorieMapping.put(0x9a, "תעודה, טבע ומדע - ספרות ושפות");
		categorieMapping.put(0x9a, "תעודה, טבע ומדע - דת");
		categorieMapping.put(0x9b, "תעודה, טבע ומדע - אנשים יוצאי דופן");
		categorieMapping.put(0x9d, "תעודה, טבע ומדע - מדיה");
		categorieMapping.put(0x9e, "תעודה, טבע ומדע - תחקיר");
		categorieMapping.put(0x9f, "תעודה, טבע ומדע - היסטוריה וארכאולוגיה");
		categorieMapping.put(0xa0, "פנאי - כללי");
		categorieMapping.put(0xa1, "פנאי - פנאי וסגנון חיים");
		categorieMapping.put(0xa2, "פנאי - עבודות יד");
		categorieMapping.put(0xa3, "פנאי - בתים ועיצובים");
		categorieMapping.put(0xa4, "פנאי - כושר ובריאות");
		categorieMapping.put(0x05, "פנאי - בישול");
		categorieMapping.put(0xa5, "פנאי - בישול");
		categorieMapping.put(0xa6, "פנאי - רכילות");
		categorieMapping.put(0xa7, "פנאי - גננות");
		categorieMapping.put(0xa8, "פנאי - יחסים");
		categorieMapping.put(0xa9, "פנאי - אופנה");
		categorieMapping.put(0xaa, "פנאי - בריאות");
		categorieMapping.put(0xab, "פנאי - ריהוט ועתיקות");
		categorieMapping.put(0xac, "פנאי - ייעוץ");
		categorieMapping.put(0xad, "פנאי - קניות");
		categorieMapping.put(0xae, "פנאי - משפחה");
		categorieMapping.put(0xaf, "פנאי - מיסטיקה");
		categorieMapping.put(0xb0, "סדרות - כללי");
		categorieMapping.put(0xb1, "סדרות - אופרת סבון, טלנובלה");
		categorieMapping.put(0xb2, "סדרות - קומדיה");
		categorieMapping.put(0xb3, "סדרות - משפחה");
		categorieMapping.put(0xb4, "סדרות - מיני סידרה");
		categorieMapping.put(0xb5, "סדרות - פעולה");
		categorieMapping.put(0xb6, "סדרות - משטרה");
		categorieMapping.put(0xb7, "סדרות - אנימציה");
		categorieMapping.put(0xb8, "סדרות - בדיוני");
		categorieMapping.put(0xb9, "סדרות - דרמה");
		categorieMapping.put(0xBA, "סדרות - ריאליטי");
		categorieMapping.put(0xbb, "סדרות - הרפתקאות");
		categorieMapping.put(0xbc, "סדרות - פעולה");
		categorieMapping.put(0xBE, "סדרות - דרמה קומית");
		categorieMapping.put(0xBF, "סדרות - אימה");
		categorieMapping.put(0xF0, "מבוגרים");
		categorieMapping.put(0xF3, "תעודה, טבע ומדע - מדיה");
		categorieMapping.put(0xF4, "בידור קל - תכנית אירוח, טוק שואו");
		categorieMapping.put(0xF5, "מבוגרים");
		categorieMapping.put(0xC0, "Pay-Per-View");
	}
}
