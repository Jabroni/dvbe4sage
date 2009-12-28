import java.util.*;

public class Programme
{
	// This is mandatory data for all programmes
	private Channel channel;												// This is the channel this programme belongs to
	private HashMap<String, String> titles;									// These are titles in all languages (the key is language, the value is the title)			
	private HashMap<String, String> descriptions;							// These are descriptions in all languages (the key is language, the value is the description)
	private Calendar startTime;												// This is the start time of the programme 
	private Calendar endTime;												// This is the end time of the programme
	
	// The data below exists only for episodes of series
	private boolean isSeries;												// TRUE is the programme is an episode of a series
	private int seasonNumber;												// This is the season number, -1 if nto given
	private int episodeNumber;												// This is the episode number, -1 if not given
	private int partNumber;													// This is the part number, -1 if not given
	
	// Other non-mandatory data
	private Vector<String> strCategories;									// String values for category
	private Vector<Integer> numCategories;									// Numeric values for category
	private int aspectRatio;												// Video aspect ratio
	private String strAspectRatio;											// String representation of aspect ratio
	private Vector<Integer> audio;											// Audio formats
	private HashMap<String, SUBTITLE_TYPE> subtitles;						// Subtitles languages and type

	public enum SUBTITLE_TYPE { SUBTITLE_TELETEXT, SUBTITLE_ONSCREEN }
	
	public Programme(Channel chan, Calendar start, Calendar stop)
	{
		isSeries = false;
		channel = chan;
		startTime = start;
		endTime = stop;
		titles = new HashMap<String, String>();
		descriptions = new HashMap<String, String>();
		strCategories = new Vector<String>();
		numCategories = new Vector<Integer>();
		audio = new Vector<Integer>();
		subtitles = new HashMap<String, SUBTITLE_TYPE>();
		seasonNumber = -1;
		episodeNumber = -1;
		partNumber = -1;
		aspectRatio = 0;
	}

	public void setIsSeries(boolean series)
	{
		isSeries = series;
	}
	
	public boolean getIsSeries()
	{
		return isSeries;
	}
	
	public int getEpisodeNumber()
	{
		return episodeNumber;
	}
	
	public void setEpisodeNumber(int number)
	{
		episodeNumber = number;
	}
	
	public int getSeasonNumber()
	{
		return seasonNumber;
	}
	
	public void setSeasonNumber(int number)
	{
		seasonNumber = number;
	}
	
	public int getPartNumber()
	{
		return partNumber;
	}
	
	public void setPartNumber(int number)
	{
		partNumber = number;
	}
	
	public void setTitle(String language, String title)
	{
		titles.put(language, title);
	}
	
	public void setDescription(String language, String desc)
	{
		descriptions.put(language, desc);
	}
		
	public void addCategory(int category)
	{
		numCategories.add(category);
	}

	public void addCategory(String category)
	{
		strCategories.add(category);
	}
	
	public void putAspectRatio(int ratio)
	{
		aspectRatio = ratio;
	}
	
	public void addSubtitles(String language, SUBTITLE_TYPE type)
	{
		subtitles.put(language, type);
	}
	
	public void addAudio(int sound)
	{
		audio.add(sound);
	}
	
	public HashMap<String, String> getTitles()
	{
		return titles;
	}
		
	public HashMap<String, String> getDescriptions()
	{
		return descriptions;
	}
	
	public Channel getChannel()
	{
		return channel;
	}

	public Calendar getStartTime()
	{
		return startTime;
	}
	
	public Calendar getEndTime()
	{
		return endTime;
	}
	
	public void adjustStartTime(int hours)
	{
		startTime.add(Calendar.HOUR_OF_DAY, hours);
	}
	
	public void adjustEndTime(int hours)
	{
		endTime.add(Calendar.HOUR_OF_DAY, hours);
	}
	
	public Vector<Integer> getNumCategories()
	{
		return numCategories;
	}
	
	public Vector<String> getStrCategories()
	{
		return strCategories;
	}
	
	public int getAspectRatio()
	{
		return aspectRatio;
	}
	
	public HashMap<String, SUBTITLE_TYPE> getSubtitles()
	{
		return subtitles;
	}
	
	public void setProgrammeLanguage(String language)
	{
		HashMap<String, String> tempStrings = new HashMap<String, String>();
		String title = titles.get(language);
		if(title != null)
			tempStrings.put(language, title);
		else
			System.out.println("Warning: a program doesn't have a title in the requested language (" + language + "), will not be in the output!");
		titles = tempStrings;
		tempStrings = new HashMap<String, String>();
		String description = descriptions.get(language);
		if(description != null)
			tempStrings.put(language, description);
		else
			System.out.println("Warning: a program doesn't have a description in the requested language (" + language + "), will not be in the output!");
		descriptions = tempStrings; 
	}
	
	public Vector<Integer> getNumericCategories()
	{
		return numCategories;
	}
	
	public Vector<String> getStringCategories()
	{
		return strCategories;
	}
	
	public void addStringCategorie(String newCategorie)
	{
		strCategories.add(newCategorie);
	}
	
	public void addNumericCategorie(int newCategorie)
	{
		numCategories.add(newCategorie);
	}
	
	public void setStrAspectRatio(String ratio)
	{
		strAspectRatio = ratio;
	}
	
	public String getStrAspectRatio()
	{
		if(strAspectRatio == null && aspectRatio != 0)
		{
			if(aspectRatio == 1 || aspectRatio == 5 || aspectRatio == 9 || aspectRatio == 13)
				strAspectRatio = new String("4:3");
			else
				strAspectRatio = new String("16:9");
		}
		return strAspectRatio;
	}
}
