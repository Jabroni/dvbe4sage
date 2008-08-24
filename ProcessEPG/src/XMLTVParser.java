import java.util.*;
import org.xml.sax.*;
import org.xml.sax.helpers.*;

public class XMLTVParser extends DefaultHandler
{
	// Global data structures
	private HashMap<Integer, Channel> channels;					// Here we keep all the channels
	private Vector<Programme> programmes;							// Here we keep all the programmes
	
	// Current variables (used for parsing)
	private Channel currentChannel;									// Current channel
	private Programme currentProgramme;								// Current programme
	
	private String currentLanguage;									// Current language
	private Programme.SUBTITLE_TYPE currentSubtitleType;			// Current subtitle type
	private String currentData;										// Current element data

	// This function parses XMLTV date and time stamps including time zones
	private Calendar parseXMLTVDateTime(String dateTime)
	{
		Calendar result = Calendar.getInstance(TimeZone.getTimeZone("GMT" + dateTime.substring(15, 20)));
		result.set(Integer.parseInt(dateTime.substring(0, 4)),
				   Integer.parseInt(dateTime.substring(4, 6)),
				   Integer.parseInt(dateTime.substring(6, 8)),
				   Integer.parseInt(dateTime.substring(8, 10)),
				   Integer.parseInt(dateTime.substring(10, 12)),
				   Integer.parseInt(dateTime.substring(12, 14)));

		return result;
	}

	public XMLTVParser()
	{
		channels = new HashMap<Integer, Channel>();
		programmes = new Vector<Programme>();
		currentData = new String();
		currentLanguage = new String();
	}
	
	public void startElement(String uri, String localName, String qName, Attributes attributes)
	{
		// Let's take care of the "channel" element and its components
		if(qName.equals("channel"))
		{
			currentChannel = new Channel(Integer.parseInt(attributes.getValue("id")));
			channels.put(currentChannel.getNumber(), currentChannel);
		}
		else if(qName.equals("display-name"))
			currentLanguage = attributes.getValue("lang");

		// Let's take care of the "pogramme" element and its components
		else if(qName.equals("programme"))
		{
			currentProgramme = new Programme(channels.get(Integer.parseInt(attributes.getValue("channel"))),
											 parseXMLTVDateTime(attributes.getValue("start")),
											 parseXMLTVDateTime(attributes.getValue("stop")));
			programmes.add(currentProgramme);
		}
		else if(qName.equals("title"))
			currentLanguage = attributes.getValue("lang");
		else if(qName.equals("desc"))
			currentLanguage = attributes.getValue("lang");
		else if(qName.equals("subtitles"))
		{
			String type = attributes.getValue("type");
			currentSubtitleType = type.equals("teletext") ? Programme.SUBTITLE_TYPE.SUBTITLE_TELETEXT : Programme.SUBTITLE_TYPE.SUBTITLE_ONSCREEN; 
		}
		else if(qName.equals("language"))
			currentLanguage = attributes.getValue("lang");
	}
	
	public void characters(char[] ch, int start, int length)
	{
		currentData = currentData + new String(ch, start, length);
	}
	
	public void endElement(String uri, String localName, String qName)
	{		
		// Take care of "channel" element closure
		if(qName.equals("icon"))
			currentChannel.putType(Integer.parseInt(currentData) == 1 ? Channel.CHANNEL_TYPE.CHANNEL_TV : Channel.CHANNEL_TYPE.CHANNEL_RADIO);
		else if(qName.equals("display-name"))
			currentChannel.addName(currentLanguage, currentData);
		
		// Take care of "programme" element closure
		else if(qName.equals("title"))
			currentProgramme.setTitle(currentLanguage, currentData);
		else if(qName.equals("desc"))
			currentProgramme.setDescription(currentLanguage, currentData);
		else if(qName.equals("category"))
			currentProgramme.addCategory(Integer.parseInt(currentData.substring(2), 16));
		else if(qName.equals("stereo"))
			currentProgramme.addAudio(Integer.parseInt(currentData.substring(2), 16));
		else if(qName.equals("aspect"))
			currentProgramme.putAspectRatio(Integer.parseInt(currentData.substring(2), 16));
		else if(qName.equals("subtitles"))
			currentProgramme.addSubtitles(currentLanguage, currentSubtitleType);
		currentData = "";
		
		/*
		if(currentProgramme != null)
		{
			System.out.println("channel=\"" + Integer.toString(currentProgramme.getChannel().getNumber()) + "\" start=\"" + Logic.getPrintableTime(currentProgramme.getStartTime()) + "\" stop=\"" + Logic.getPrintableTime(currentProgramme.getEndTime()) + "\"");
		}*/
	}
	
	public HashMap<Integer, Channel> getChannels()
	{
		return channels;
	}
	
	public Vector<Programme> getProgrammes()
	{
		return programmes;
	}
}
