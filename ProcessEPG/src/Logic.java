import java.util.*;
import javax.xml.transform.sax.*;
import org.xml.sax.helpers.*;
import org.xml.sax.*;
import java.text.*;
import java.io.*;
import java.util.regex.*;

public class Logic
{
	// Here we keep all the data
	private HashMap<Integer, Channel> allChannels;
	private Vector<Programme> allProgrammes;
	
	// For unused categories
	private HashSet<Integer> unusedCategories;
	
	// Here we keep the configuration
	private Configuration config; 
	
	static public String getPrintableTime(Calendar time)
	{
		int offset = time.getTimeZone().getRawOffset() / 3600000;
		String sign = offset >= 0 ? new String("+") : new String("-");
		offset = Math.abs(offset);
		DecimalFormat myFormat = new DecimalFormat("00");
		return Integer.toString(time.get(Calendar.YEAR)) + 
			   myFormat.format(time.get(Calendar.MONTH) + 1) +
			   myFormat.format(time.get(Calendar.DAY_OF_MONTH)) +
			   myFormat.format(time.get(Calendar.HOUR_OF_DAY)) +
			   myFormat.format(time.get(Calendar.MINUTE)) +
			   myFormat.format(time.get(Calendar.SECOND)) + " " +
		       sign + myFormat.format(offset) + "00";
	}
	
	public Logic(HashMap<Integer, Channel> channels, Vector<Programme> programmes) throws FileNotFoundException
	{
		allChannels = channels;
		allProgrammes = programmes;
		config = new Configuration();
		config.init("processepg.ini");
		unusedCategories = new HashSet<Integer>();
	}
	
	public boolean wantsCreateXMLTVImporterChannelsFile()
	{
		return config.createXMLTVImporterChannelsFile;
	}
	
	// This function converts Latin (Roman) numbers to conventional decimals 
	private static int convertFromLatin(String latinNumber)
	{
		int result = 0;
		for(int i = 0; i < latinNumber.length(); i++ )
		{
			char curChar = latinNumber.charAt(i);
			char prevChar = i > 0 ? latinNumber.charAt(i - 1) : ' ';
			char nextChar = i < (latinNumber.length() - 1) ? latinNumber.charAt(i + 1) : ' ';
			switch(curChar)
			{
				case 'M':
					result += prevChar == 'C' ? 900 : 1000;
					break;
				case 'C':
					if(nextChar == 'M')
						continue;
					result += prevChar == 'X' ? 90 : 100;
					break;
				case 'L':
					result += prevChar == 'X' ? 40 : 50;
					break;
				case 'X':
					if(nextChar == 'C' || nextChar == 'L')
						continue;
					result += prevChar == 'I' ? 9 : 10;
					break;
				case 'V':
					result += prevChar == 'I' ? 4 : 5;
					break;
				case 'I':
					if(nextChar == 'X' || nextChar == 'V')
						continue;
					result += 1;
					break;
			}
		}
		
		return result;
	}
	
	// We assume the season number cannot be more than 20
	private boolean suspectedNonNumber(int number)
	{
		return number > 20;
	}
	
	// Parse series info and update programme if necessary
	private boolean updateProgramme(Programme programme,			// Programme we work with
									String language,				// Language we're currently analyzing
									String originalTitle,			// The original title of the programme
									boolean searchInTitle,			// True if we search in title, otherwise we search in description
									boolean updateTitle,			// True if we want to update the programme title
									String patternString,			// Regex pattern for search
									int seasonGroup,				// Regex group where we expect to find the season info (-1 if we don't expect to find it)
									int episodeGroup,				// Regex group where we expect to find the episode info (-1 if we don't expect to find it)
									int partGroup,					// Regex group where we expect to find the part info (-1 if we don't expect to find it)
									boolean latinSeason)			// True if season number expected to be a Latin (Roman) number
	{
		// First, build pattern based on the pattern string
		Pattern pattern = Pattern.compile(patternString);
		// Now, figure out what we want to match against the pattern
		String whatToMatch = searchInTitle ? originalTitle : programme.getDescriptions().get(language);
		// Check if it's not null
		if(whatToMatch != null)
		{
			// Build the matcher
			Matcher matcher = pattern.matcher(whatToMatch);
			// If we got the match, process components
			if(matcher != null && matcher.matches())
			{
				// Process seasons first
				if(seasonGroup != -1 && programme.getSeasonNumber() == -1)
				{
					// Get the season number
					String number = matcher.group(seasonGroup);
					// Convert from Latin if necessary
					int seasonNumber = latinSeason ? Logic.convertFromLatin(number) : Integer.parseInt(number);
					// Update only if it's really season, otherwise go away (we'll catch episode later if relevant)
					if(!suspectedNonNumber(seasonNumber))
						programme.setSeasonNumber(seasonNumber);
					else
						return false;
				}
				// Process episodes
				if(episodeGroup != -1 && programme.getEpisodeNumber() == -1)
					programme.setEpisodeNumber(Integer.parseInt(matcher.group(episodeGroup)));
				// Process parts
				if(partGroup != -1 && programme.getPartNumber() == -1)
					programme.setPartNumber(Integer.parseInt(matcher.group(partGroup)));
				// Update title if necessary
				if(searchInTitle && updateTitle)
					programme.setTitle(language, matcher.group(1));
				// Finally, indicate that we have series here
				programme.setIsSeries(true);
				return true;
			}
			else
				return false;
		}
		return false;
	}
	
	// This is the main processing function of the logic
	public void process()
	{	
		// No, let's iterate through all the programmes
		for(int i = 0; i < allProgrammes.size(); i++)
		{
			// For each programme
			Programme programme = allProgrammes.get(i);
			
			// Process categories
			boolean shouldTreatAsSeries = false;
			Vector<Integer> categories = programme.getNumCategories();
			for(int j = 0; j < categories.size(); j++)
			{
				int numCategorie = categories.get(j);
				shouldTreatAsSeries |= config.shouldTreatAsSeries(numCategorie, programme.getChannel().getNumber());
				String strCategorie = config.categorieMapping.get(numCategorie); 
				if(strCategorie != null)
					programme.addStringCategorie(strCategorie);
				else
					unusedCategories.add(numCategorie);
			}
			
			// If should treat as series
			if(shouldTreatAsSeries)
			{
				String hebrewTitle = programme.getTitles().get("he");
				//String originalHebrewTitle = hebrewTitle;
				if(config.getLanguage(programme.getChannel().getNumber()).equals("he") && hebrewTitle != null && !hebrewTitle.equalsIgnoreCase(programme.getChannel().getNames().get("he")))
				{
					// Sanity check (for clock)
					Pattern pattern = Pattern.compile(".*שעה\\s*\\d?\\d:\\d\\d\\s*");	
					Matcher matcher = pattern.matcher(hebrewTitle);
					if(matcher != null && matcher.matches())
						continue;
					
					// Get rid of "episode of the end of the season" first and overwrite the title
					if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*פ(?:רק|')?\\s+ס(?:יום|')\\s+ה?עונה\\s*", -1, -1, -1, false))
					{
						hebrewTitle = programme.getTitles().get("he");
						programme.setSeasonNumber(-2);
					}
					
					// Get rid of "end of the season" first and overwrite the title
					if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ס(?:יום|')?\\s+ה?עונה\\s*", -1, -1, -1, false))
						hebrewTitle = programme.getTitles().get("he");
					
					// Taking care of season first
					// The word "season" is present and it has an Arabic number 
					if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*עונה\\s*ה?\\s*-?\\s*(\\d++).*?", 2, -1, -1, false))
						;
					// The word "season" is present and it has a Latin number
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*עונה\\s*ה?\\s*-?\\s*([MCLXVI]++).*?", 2, -1, -1, true))
						;
					// The word "season" is present, has no number, says "new season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?חדשה.*?", -1, -1, -1, false))
						programme.setSeasonNumber(-2);
					// The word "season" is present, has no number, says "first season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?ראשונה.*?", -1, -1, -1, false))
						programme.setSeasonNumber(1);
					// The word "season" is present, has no number, says "second season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?שני?יה.*?", -1, -1, -1, false))
						programme.setSeasonNumber(2);
					// The word "season" is present, has no number, says "third season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?שלי?שית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(3);
					// The word "season" is present, has no number, says "fourth season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?רביעית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(4);
					// The word "season" is present, has no number, says "fifth season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?שמי?שית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(5);
					// The word "season" is present, has no number, says "sixth season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?שי?שית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(6);
					// The word "season" is present, has no number, says "seventh season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?שביעית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(7);
					// The word "season" is present, has no number, says "eights season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?שמי?נית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(8);
					// The word "season" is present, has no number, says "ninth season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?תשי?עית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(9);
					// The word "season" is present, has no number, says "tenth season"
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*ה?עונה\\s+ה?עשירית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(10);
					// Now taking care of season number when the word "season" is not present but the word "episode" is
					// Season has Arabic number
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*(\\d++)\\s*[:-]?\\s*פ(?:רק|')?.*?", 2, -1, -1, false))
						;
					// Season has Latin number
					else if(updateProgramme(programme, "he", hebrewTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*([MCLXVI]++)\\s*[:-]?\\s*פ(?:רק|')?.*?", 2, -1, -1, true))
						;
					
					// Now taking care of episodes regardless of season, need to update title only if season was not found beforehand
					// The episode has number
					if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*[:-]?\\s*פ(?:רק|')?\\s*[:-]?\\s*(\\d++).*?", -1, 2, -1, false))
						;
					// The final episode
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*[:-]?\\s*פ(?:רק|')?\\s+ה?סיום.*?", -1, -1, -1, false))
						programme.setEpisodeNumber(-2);
					// The last episode
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*[:-]?\\s*פ(?:רק|')?\\s+ה?אחרון.*?", -1, -1, -1, false))
						programme.setEpisodeNumber(-2);
					// The first episode
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*[:-]?\\s*פ(?:רק|')?\\s+ה?ראשון.*?", -1, -1, -1, false))
						programme.setEpisodeNumber(1);
					// The second episode
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*[:-]?\\s*פ(?:רק|')?\\s+ה?שני.*?", -1, -1, -1, false))
						programme.setEpisodeNumber(2);
					// Now for the clause both "episode" and "season" words are absent
					// Both season and episode have numbers (Arabic for season)
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*(\\d++)\\s*[:-]?\\s*(\\d++)\\s*", 2, 3, -1, false))
						;
					// Both season and episode have numbers (Latin for season)
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*([MCLXVI]++)\\s*[:-]?\\s*(\\d++)\\s*", 2, 3, -1, true))
						;
					// Only the episode has number
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*:\\s*(\\d++)\\s*", -1, 2, -1, false))
						;
					// Only the season has number (Arabic)
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s*(?:-|\\s+)\\s*(\\d++)\\s*", 2, -1, -1, false))
						;
					// Only the season has number (Latin)
					else if(updateProgramme(programme, "he", hebrewTitle, true, programme.getSeasonNumber() == -1, "\\s*(.+?)\\s+([MCLXVI]++)\\s*", 2, -1, -1, true))
						;
					
					// Now for descriptions
					if(updateProgramme(programme, "he", null, false, false, ".*?פ(?:רק|')?\\s*[:-]?\\s*(\\d++).*?", -1, 1, -1, false))
						;
					else if(updateProgramme(programme, "he", null, false, false, ".*?פ(?:רק|')?\\s+ה?ראשון.*?", -1, -1, -1, false))
						programme.setEpisodeNumber(1);
					else if(updateProgramme(programme, "he", null, false, false, ".*?פ(?:רק|')?\\s+ה?שני.*?", -1, -1, -1, false))
						programme.setEpisodeNumber(2);
					
					if(updateProgramme(programme, "he", null, false, false, ".*?עונה\\s*[:-]?\\s*(\\d++).*?", 1, -1, -1, false))
						;
					else if(updateProgramme(programme, "he", null, false, false, ".*?עונה\\s*ה?\\s*-\\s*(\\d++).*?", 1, -1, -1, false))
						;
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?ראשונה.*?", -1, -1, -1, false))
						programme.setSeasonNumber(1);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?שני?יה.*?", -1, -1, -1, false))
						programme.setSeasonNumber(2);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?שלי?שית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(3);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?רביעית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(4);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?חמי?שית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(5);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?שי?שית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(6);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?שבי?עית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(7);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?שמי?נית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(8);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?תשי?עית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(9);
					else if(updateProgramme(programme, "he", null, false, false, ".*?ה?עונה\\s+ה?עשירית.*?", -1, -1, -1, false))
						programme.setSeasonNumber(10);
					
					updateProgramme(programme, "he", null, false, false, ".*?חלק\\s*[:-]?\\s*(\\d++).*?", -1, -1, 1, false);
					
					// Last is the part number (only in descriptions)
					if(updateProgramme(programme, "he", null, false, false, ".*חלק\\s+א'.*?", -1, -1, -1, false))
						programme.setPartNumber(1);
					else if(updateProgramme(programme, "he", null, false, false, ".*חלק\\s+ב'.*?", -1, -1, -1, false))
						programme.setPartNumber(2);
					else if(updateProgramme(programme, "he", null, false, false, ".*חלק\\s+ג'.*?", -1, -1, -1, false))
						programme.setPartNumber(3);
					else if(updateProgramme(programme, "he", null, false, false, ".*חלק\\s+ד'.*?", -1, -1, -1, false))
						programme.setPartNumber(4);
				}
				
				// Now try to use Russian info
				String russianTitle = programme.getTitles().get("ru");
				if(russianTitle != null && !russianTitle.equalsIgnoreCase(programme.getChannel().getNames().get("ru")))
				{
					// "Episode" word is present but "season" is not, both have numbers, Ч.
					if(updateProgramme(programme, "ru", russianTitle, true, true, "\\s*(.+?)\\s+(\\d++)\\s*[:-]?\\s*[ЧчСсCc]\\.?\\s*(\\d++).*?", 2, 3, -1, false))
						;
					// "Episode" word is present but "season" is not, both have numbers, -я с.
					else if(updateProgramme(programme, "ru", russianTitle, true, true, "\\s*(.+?)\\s+(\\d++)\\s*[:-]?\\s*(\\d++)(\\s*-\\s*я\\s*)?[cс]\\.?.*?", 2, 3, -1, false))
						;
					// "Episode" word is present but "season" is not, both have numbers, -я серия
					else if(updateProgramme(programme, "ru", russianTitle, true, true, "\\s*(.+?)\\s+(\\d++)\\s*[:-]?\\s*(\\d++)(\\s*-\\s*я\\s*)?серия.*?", 2, 3, -1, false))
						;
					// "Episode" word is present but "season" is not, only episode has number, Ч.
					else if(updateProgramme(programme, "ru", russianTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*[ЧчСсCc]\\.?\\s*(\\d++).*?", -1, 2, -1, false))
						;
					// "Episode" word is present but "season" is not, only episode has number, -я с.
					else if(updateProgramme(programme, "ru", russianTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*(\\d++)(\\s*-\\s*я\\s*)?[cс]\\.?.*?", -1, 2, -1, false))
						;
					// "Episode" word is present but "season" is not, only episode has number, -я серия
					else if(updateProgramme(programme, "ru", russianTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*(\\d++)(\\s*-\\s*я\\s*)?\\s+серия.*?", -1, 2, -1, false))
						;
					// Both "episode" and "season" words are not present but the season has number
					else if(updateProgramme(programme, "ru", russianTitle, true, true, "\\s*(.+?)\\s*-?\\s*(\\d++)\\s*", 2, -1, -1, false))
						;
					// Now for descriptions
					updateProgramme(programme, "ru", null, false, false, ".*?\\s+[Чч]p\\.?\\s*(\\d++).*?", -1, 1, -1, false);
					updateProgramme(programme, "ru", null, false, false, ".*?\\s*(\\d++)(\\s*-\\s*я\\s*)?\\s+с\\.?.*?", -1, 1, -1, false);
					updateProgramme(programme, "ru", null, false, false, ".*?\\s*(\\d++)(\\s*-\\s*я\\s*)?\\s+серия.*?", -1, 1, -1, false);
				}
				
				// Last, process English stuff
				String englishTitle = programme.getTitles().get("en");
				if(englishTitle != null  && !englishTitle.equalsIgnoreCase(programme.getChannel().getNames().get("en")))
				{ 
					// "Episode" word is present but "season" is not, both have numbers, the season has Arabic number, ep.
					if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+(\\d++)\\s*[:-]?\\s*(?:[Ee]p|[Pp]t)\\.\\s*(\\d++).*?", 2, 3, -1, false))
						;
					// "Episode" word is present but "season" is not, both have numbers, the season has Latin number, ep.
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+([MCLXVI]++)\\s*[:-]?\\s*(?:[Ee]p|[Pp]t)\\.\\s*(\\d++).*?", 2, 3, -1, true))
						;
					// "Episode" word is present but "season" is not, both have numbers, the season has Arabic number, episode
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+(\\d++)\\s*[:-]?\\s*[Ee]pisode\\s*(\\d++).*?", 2, 3, -1, false))
						;
					// "Episode" word is present but "season" is not, both have numbers, the season has Latin number, episode
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+([MCLXVI]++)\\s*[:-]?\\s*[Ee]pisode\\s*(\\d++).*?", 2, 3, -1, true))
						;
					// "Episode" word is present but "season" is not, and the season doesn't have a number, ep.
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*(?:[Ee]p|[Pp]t)\\.\\s*(\\d++).*?", -1, 2, -1, false))
						;
					// "Episode" word is present but "season" is not, and the season doesn't have a number, episode
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s*[:-]?\\s*[Ee]pisode\\s*(\\d++).*?", -1, 2, -1, false))
						;
					// Both "episode" and "season" words are not present but the season has Arabic number, episode has number
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+(\\d++)\\s*[:-]?\\s*(\\d++)\\s*", 2, 3, -1, false))
						;
					// Both "episode" and "season" words are not present but the season has Latin number, episode has number
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+([MCLXVI]++)\\s*[:-]?\\s*(\\d++)\\s*", 2, 3, -1, true))
						;
					// Both "episode" and "season" words are not present but the season has Latin number, no episode number
					else if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+([MCLXVI]++)\\s*", 2, -1, -1, true))
						;
					// Both "episode" and "season" words are not present but the season has Arabic number, no episode number
					// Here we assume that if the season is "unknown", than the number is of the episode, otherwise of the season
					// This is a problematic case since sometimes the bastards don't put ep. prefix for episodes, so we need to have a sanity check
					else
					{
						int prevSeasonNumber = programme.getSeasonNumber();
						int prevEpisodeNumber = programme.getEpisodeNumber();
						if(updateProgramme(programme, "en", englishTitle, true, true, "\\s*(.+?)\\s+(\\d++)\\s*", 2, -1, -1, false))
						{
							// Here we wrongly assumed season has a number while if fact this was and episode number
							if(prevSeasonNumber < 0 && prevEpisodeNumber > 0 && prevEpisodeNumber == programme.getSeasonNumber())
								programme.setSeasonNumber(-1);
						}
					}
					
					updateProgramme(programme, "en", null, false, false, ".*?\\s+(\\d++)\\s*[:-]?\\s*(?:[Ee]p|[Pp]t)\\.\\s*(\\d++).*?", 1, 2, -1, false);
					updateProgramme(programme, "en", null, false, false, ".*?\\s+(?:[Ee]p|[Pp]t)\\.\\s*(\\d++).*?", -1, 1, -1, false); 
				}
				
				// Take care of wide screen indicator
				Pattern pattern = Pattern.compile(".*?מ(?:סך|\\.)\\s+רחב.*?");
				String hebrewDescription = programme.getDescriptions().get("he");
				if(hebrewDescription != null)
				{
					Matcher matcher = pattern.matcher(hebrewDescription);
					if(matcher != null && matcher.matches())
						programme.setStrAspectRatio("16:9");
				}
			}
		}

		// For starters, let's determine the main language for each channel
		Iterator<Channel> channels = allChannels.values().iterator();
		while(channels.hasNext())
		{
			// Get the current channel
			Channel channel = channels.next();
			
			// Determine its language based on configuration
			if(config.forceRussian.contains(channel.getNumber()))
				channel.setChannelLanguage("ru");
			else if(config.forceEnglish.contains(channel.getNumber()))
				channel.setChannelLanguage("en");
			else if(config.forceHebrew.contains(channel.getNumber()))
				channel.setChannelLanguage("he");
			else if(config.forceArabic.contains(channel.getNumber()))
				channel.setChannelLanguage("ar");
			else
				channel.setChannelLanguage(config.defaultLanguage);
		}
		
		// Now, let's iterate through all the programmes once again
		for(int i = 0; i < allProgrammes.size(); i++)
		{
			// For each programme
			Programme programme = allProgrammes.get(i);
			
			// Now we can eliminate unnecessary programme titles and descriptions
			programme.setProgrammeLanguage(programme.getChannel().getChannelLanguage());
			
			// And adjust times
			programme.adjustStartTime(config.gmtOffset);
			programme.adjustEndTime(config.gmtOffset);
		}
		
		// Now, let's print all unused categories
		Iterator<Integer> it = unusedCategories.iterator();
		while(it.hasNext())
			System.out.println("Warning: unused category \"0x" + Integer.toHexString(it.next()) + "\"!");
	}
	
	public void outputChannelsXML(TransformerHandler handler) throws SAXException
	{
		// Start the output document
		handler.startDocument();
				
		// Create attributes list for the root "lineups" node
		AttributesImpl lineupsAttributes = new AttributesImpl();
		
		// Add the "version=2" attribute to the list of attributes
		lineupsAttributes.addAttribute(null, null, "version", "CDATA", "2");
		
		// Write the root "lineups" element with its attributes
		handler.startElement(null, null, "lineups", lineupsAttributes);
		
		// Now create attributes for "lineup" element
		AttributesImpl lineupAttributes = new AttributesImpl();
		
		// Add the "id=1" attribute to the list
		lineupAttributes.addAttribute(null, null, "id", "CDATA", "1");
		
		// Add the "Name=Yes" attribute to the list
		lineupAttributes.addAttribute(null, null, "name", "CDATA", "Yes");
		
		// Add the "Country=ISR" attribute to the list
		lineupAttributes.addAttribute(null, null, "country", "CDATA", "ISR");
		
		// Add the "Region=" attribute to the list
		lineupAttributes.addAttribute(null, null, "region", "CDATA", "");
		
		// Add the "Provider=YES" attribute to the list
		lineupAttributes.addAttribute(null, null, "provider", "CDATA", "YES");
		
		// Add the "tv-standard=DVB" attribute to the list
		lineupAttributes.addAttribute(null, null, "tv-standard", "CDATA", "DVB");
		
		// Add the "tv-source=Satellite" attribute to the list
		lineupAttributes.addAttribute(null, null, "ts-source", "CDATA", "Satellite");
		
		// Add the "set-top-box=false" attribute to the list
		lineupAttributes.addAttribute(null, null, "set-top-box", "CDATA", "false");
		
		// Add the "private=false" attribute to the list
		lineupAttributes.addAttribute(null, null, "private", "CDATA", "false");
		
		// Write the single "lineup" element with its attributes
		handler.startElement(null, null, "lineup", lineupAttributes);
		
		// Now, output the channels
		Iterator<Channel> channels = allChannels.values().iterator();
		while(channels.hasNext())
		{
			// Get the current channel
			Channel channel = channels.next();
			
			// Fill its attributes
			AttributesImpl attributes = new AttributesImpl();
			
			// Add the "id" attribute
			attributes.addAttribute(null, null, "id", "CDATA", Integer.toString(channel.getNumber()));
			
			// This is channel name as we want it
			String name = channel.getNames().get(channel.getChannelLanguage());
			
			// Add the "name" attribute
			attributes.addAttribute(null, null, "name", "CDATA", name);
			
			// Add the "call-sign" attribute
			attributes.addAttribute(null, null, "call-sign", "CDATA", name);
			
			// Add the "number" attribute
			attributes.addAttribute(null, null, "number", "CDATA", Integer.toString(channel.getNumber()));
			
			// Add the "time-offset" attribute
			attributes.addAttribute(null, null, "time-offset", "CDATA", "0");
			
			// Add the "xmltv-id" attribute
			attributes.addAttribute(null, null, "xmltv-id", "CDATA", Integer.toString(channel.getNumber()));
			
			// Add the "allow-hd" attribute
			attributes.addAttribute(null, null, "allow-hd", "CDATA", config.isHDChannel(channel.getNumber()) ? "True" : "False");
			
			// And write the "channel" tag
			handler.startElement(null, null, "channel", attributes);
			
			// End the "channel" tag
			handler.endElement(null, null, "channel");
		}
		
		// Close the "lineup" element
		handler.endElement(null, null, "lineup");
		
		// Close the "lineups" element
		handler.endElement(null, null, "lineups");
		
		// End the output document
		handler.endDocument();
	}
	
	public void output(TransformerHandler handler) throws SAXException
	{
		// Start the output document
		handler.startDocument();
		
		// Write the DTD
		handler.startDTD("tv", null, "xmltv.dtd");
		
		// Write the root "tv" element with empty attributes
		handler.startElement(null, null, "tv", null);
		
		// First, output the channels
		Iterator<Channel> channels = allChannels.values().iterator();
		while(channels.hasNext())
		{
			// Get the current channel
			Channel channel = channels.next();
					
			// Fill its attributes (only id) and write the tag
			AttributesImpl attributes = new AttributesImpl();
			attributes.addAttribute(null, null, "id", "CDATA", Integer.toString(channel.getNumber()));
			handler.startElement(null, null, "channel", attributes);
			
			// Do this for all existing names
			HashMap<String, String> names = channel.getNames();
			Iterator<String> nameLanguages = names.keySet().iterator();
			while(nameLanguages.hasNext())
			{
				// Now start the "display-name" tag
				attributes.clear();
				String language = nameLanguages.next();
				attributes.addAttribute(null, null, "lang", "CDATA", language);
				handler.startElement(null, null, "display-name", attributes);
				
				// Write the name of the channel
				String name = names.get(language);
				handler.characters(name.toCharArray(), 0, name.length());
				
				// End the "display-name" tag
				handler.endElement(null, null, "display-name");				
			}
				
			// End the "channel" tag
			handler.endElement(null, null, "channel");
		}

		// Second, output the programmes
		for(int i = 0; i < allProgrammes.size(); i++)
		{
			// Get the current programme
			Programme programme = allProgrammes.get(i);
			
			// Do output only if has at least one title and description
			if(programme.getTitles().size() != 0 && programme.getDescriptions().size() != 0)
			{
			
				// Fill its attributes ("channel", "start" and "stop") and write the tag
				AttributesImpl attributes = new AttributesImpl();
				attributes.addAttribute(null, null, "channel", "CDATA", Integer.toString(programme.getChannel().getNumber()));
				attributes.addAttribute(null, null, "start", "CDATA", getPrintableTime(programme.getStartTime()));
				attributes.addAttribute(null, null, "stop", "CDATA", getPrintableTime(programme.getEndTime()));
				handler.startElement(null, null, "programme", attributes);
				
				// Do this for all existing titles
				HashMap<String, String> titles = programme.getTitles();
				Iterator<String> titleLanguages = titles.keySet().iterator();
				while(titleLanguages.hasNext())
				{
					// Now print the "title" tag 
					attributes.clear();
					String language = titleLanguages.next(); 
					attributes.addAttribute(null, null, "lang", "CDATA", language);
					handler.startElement(null, null, "title", attributes);
					
					// And the title itself
					String title = titles.get(language);
					handler.characters(title.toCharArray(), 0, title.length());
					
					// And finish the tag
					handler.endElement(null, null, "title");
				}
				
				// Do this for all existing descriptions
				HashMap<String, String> descriptions = programme.getDescriptions();
				Iterator<String> descLanguages = descriptions.keySet().iterator();
				while(descLanguages.hasNext())
				{
					// Now print the "desc" tag 
					attributes.clear();
					String language = descLanguages.next();
					attributes.addAttribute(null, null, "lang", "CDATA", language);
					handler.startElement(null, null, "desc", attributes);
					
					// And the title itself
					String description = descriptions.get(language);
					handler.characters(description.toCharArray(), 0, description.length());
					
					// And finish the tag
					handler.endElement(null, null, "desc");
				}
				
				// Now, output string categories
				Vector<String> strCategories = programme.getStrCategories();
				for(int k = 0; k < strCategories.size(); k++)
				{
					// Now print the "category" tag 
					handler.startElement(null, null, "category", null);
					
					// And the category itself
					String category = strCategories.get(k);
					handler.characters(category.toCharArray(), 0, category.length());
					
					// And finish the tag
					handler.endElement(null, null, "category");
				}
				
				// Take care of episodes
				if(programme.getIsSeries())
				{
					// Now print the "episode-num" tag 
					attributes.clear(); 
					attributes.addAttribute(null, null, "system", "CDATA", "xmltv_ns");
					handler.startElement(null, null, "episode-num", attributes);
					
					// And the episode parts
					if(programme.getSeasonNumber() > 0)
					{
						String seasonNumber = Integer.toString(programme.getSeasonNumber() - 1);
						handler.characters(seasonNumber.toCharArray(), 0, seasonNumber.length());
					}
					
					String dot = new String(".");
					handler.characters(dot.toCharArray(), 0, dot.length());
					
					if(programme.getEpisodeNumber() > 0)
					{
						String espisodeNumber = Integer.toString(programme.getEpisodeNumber() - 1);
						handler.characters(espisodeNumber.toCharArray(), 0, espisodeNumber.length());
					}
					
					handler.characters(dot.toCharArray(), 0, dot.length());
					
					if(programme.getPartNumber() > 0)
					{
						String partNumber = Integer.toString(programme.getPartNumber() - 1);
						handler.characters(partNumber.toCharArray(), 0, partNumber.length());
					}
					
					// And finish the tag
					handler.endElement(null, null, "episode-num");
				}
				
				// Now for video
				String aspectRatio = programme.getStrAspectRatio() ;
				if(aspectRatio != null)
				{
					// Start "video" element
					handler.startElement(null, null, "video", null);
					
					// Start "aspect" element
					handler.startElement(null, null, "aspect", null);
					
					// Output the aspect itself
					handler.characters(aspectRatio.toCharArray(), 0, aspectRatio.length());
					
					// End "aspect" element
					handler.endElement(null, null, "aspect");
					
					// End "video" element
					handler.endElement(null, null, "video");
				}
	
				// Finally, close the "programme" tag
				handler.endElement(null, null, "programme");
			}
		}
		
		// Close the "tv" element
		handler.endElement(null, null, "tv");
		
		// End the output document
		handler.endDocument();
	}
}
