import org.xml.sax.SAXException;
import javax.xml.parsers.*;
import java.io.*;
import javax.xml.transform.*;
import javax.xml.transform.stream.*;
import javax.xml.transform.sax.*;

public class ProcessEPG
{
	public static void main(String[] args) throws ParserConfigurationException, TransformerConfigurationException, TransformerException, TransformerFactoryConfigurationError
	{
		if(args.length != 2)
		{
			System.out.println("Usage: java ProcessEPG <input_file> <output_file>");
			return;
		}
		try
		{
			SAXParser parser = SAXParserFactory.newInstance().newSAXParser();
			XMLTVParser myParser = new XMLTVParser();
			parser.parse(new File(args[0]), myParser);
			
			Logic logic = new Logic(myParser.getChannels(), myParser.getProgrammes());
			
			logic.process();
			
			// Create an appropriate transformer and save the document
			TransformerHandler outputHandler = ((SAXTransformerFactory)SAXTransformerFactory.newInstance()).newTransformerHandler();
			Transformer outputSerializer = outputHandler.getTransformer();
			outputSerializer.setOutputProperty(OutputKeys.ENCODING,"UTF-8");
			outputSerializer.setOutputProperty(OutputKeys.DOCTYPE_SYSTEM, "xmltv.dtd");
			outputSerializer.setOutputProperty(OutputKeys.INDENT, "yes");
			outputHandler.setResult(new StreamResult(new FileOutputStream(args[1])));
			
			try
			{
				logic.output(outputHandler);
				
				if(logic.wantsCreateXMLTVImporterChannelsFile())
				{
					// Create an appropriate transformer and save the document
					TransformerHandler channelsHandler = ((SAXTransformerFactory)SAXTransformerFactory.newInstance()).newTransformerHandler();
					Transformer channelSerializer = channelsHandler.getTransformer();
					channelSerializer.setOutputProperty(OutputKeys.ENCODING,"UTF-8");
					channelSerializer.setOutputProperty(OutputKeys.INDENT, "yes");
					String channelsXMLPath = System.getenv("ALLUSERSPROFILE") + "\\Application Data\\LM Gestion\\SageTV XMLTV Importer\\lineups.xml";
					System.out.println("Writing " + channelsXMLPath + " file!");
					channelsHandler.setResult(new StreamResult(new FileOutputStream(channelsXMLPath)));
					
					// Output channels XML
					logic.outputChannelsXML(channelsHandler);
				}
			}
			catch (SAXException e)
			{
				System.out.println("Error while outputting the file!");
			}
		}
		catch (SAXException e)
		{
			System.out.println("Error while parsing the input file!");
		}
		catch (IOException e)
		{
			System.out.println("Error: can't open the file!");
		}
	}
}
