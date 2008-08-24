import java.util.*;

public class Channel
{
	private int channelNumber;									// This is a unique channel number
	private HashMap<String, String> names;						// These are channel multilingual names
	private CHANNEL_TYPE type;									// This is a channel type
	private String channelLanguage;								// This is non-null only when a single language is set
	
	public enum CHANNEL_TYPE { CHANNEL_TV, CHANNEL_RADIO }
	
	public Channel(int number)
	{
		channelNumber = number;
		names = new HashMap<String, String>();
	}
	
	public void addName(String language, String name)
	{
		names.put(language, name);
	}

	public HashMap<String, String> getNames()
	{
		return names;
	}
	
	public void putType(CHANNEL_TYPE channelType)
	{
		type = channelType;
	}
	
	public CHANNEL_TYPE getChannelType()
	{
		return type;
	}
	
	public void setChannelLanguage(String language)
	{
		channelLanguage = language;
		HashMap<String, String> tempNames = new HashMap<String, String>();
		tempNames.put(language, names.get(language));
		names = tempNames;
	}
	
	public String getChannelLanguage()
	{
		return channelLanguage;
	}

	public int getNumber()
	{
		return channelNumber;
	}
}
