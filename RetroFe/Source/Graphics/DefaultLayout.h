/* This file is part of RetroFE.
 *
 * Embedded Fail-Safe Default Layout (BaseCenter)
 * Used when no layout.xml or splash.xml is found on disk.
 */
#pragma once

#include <string>

namespace DefaultLayout
{

// Part 1: Horizontal menu and containers (~9.7 KB)
static const char kLayoutXmlPart1[] = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<layout width="1920" height="1080" font="fonts/Arial.ttf" loadFontSize="30" fontColor="ffffff">
 
  <!-- ======================================================== -->
  <!-- ======================================================== -->
  <!-- BARRAS / LINHAS DO MENU HORIZONTAL                       -->
  <!-- ======================================================== -->

   <!-- Linha laranja superior top (a 3px de distância - 2px de altura) -->



   <!-- Linha laranja superior bottom (a 3px de distância - 2px de altura) -->
  <container backgroundColor="FFA500" backgroundAlpha="1" x="0" y="450" width="1920" height="4" layer="5">
      <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
      <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
      <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
      <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
	    </container>
		
		  <!-- Linha cinza centro (por cima do menu - 175px de altura) -->
  <container backgroundColor="808080" backgroundAlpha="0.5" x="0" y="452" width="1920" height="175" layer="3">
      <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
      <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
      <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
      <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
      <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
  </container>
		
	<!-- Linha laranja inferior top (a 3px de distância - 2px de altura) -->
	  <container backgroundColor="FFA500" backgroundAlpha="1" x="0" y="627" width="1920" height="4" layer="5">
		  <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
		  <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
		  <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
		  <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
		  <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
	  </container>	
		



 	 <!--Menu Horizontal-->
  <menu  menuIndex="0" type="custom" imageType="logo" scrollTime=".2" scrollAcceleration=".1" minScrollTime="0" orientation="horizontal" scaleMode="fit" layer="5">
        <itemDefaults alpha="1" x="center" y="center" width="100" xOrigin="center" yOrigin="center" fontSize="30" maxWidth="100" autoFit="true" maxFontSize="30" layer="5"/>
        <item xOffset="-1240" alpha="0"/>
        <item xOffset="-950">
             <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="-760">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="-570">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="-380">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="-190">
             <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="0" alpha="1" width="125" selected="true">
		    <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="190">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="380">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="570">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="760">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="950">
            <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
            <onGameEnter menuIndex="0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
            <onGameExit  menuIndex="0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
        </item>
        <item xOffset="1240" alpha="0"/>
    </menu>
	 
  <!-- ======================================================== -->
  <!-- BARRAS / LINHAS E SHAPES COM CURVA DO MENU WHEEL         -->
  <!-- Wheel vertical no lado direito: curvePosition="right"    -->
  <!-- ======================================================== -->


  )xml";

// Part 2: Vertical wheel menu, curve containers and fanarts (~7.5 KB)
static const char kLayoutXmlPart2[] = R"xml(<!-- Linha de contorno laranja com curva a ladear o wheel vertical a direita -->
  <container shape="curve" curvePosition="right" curveMode="line" x="1525" y="0" width="4" height="1080" curveDepth="150" backgroundColor="FFA500" alpha="0" layer="5">
      <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
      <onMenuEnter menuIndex=">0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
      <onMenuExit  menuIndex=">0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
      <onGameEnter menuIndex=">0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
      <onGameExit  menuIndex=">0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
  </container>
  
    <container shape="curve" curvePosition="right" curveMode="line" x="1529" y="0" width="325" height="1080" curveDepth="150" backgroundColor="808080" backgroundAlpha="0.5" alpha="0" layer="3">
      <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
      <onMenuEnter menuIndex=">0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
      <onMenuExit  menuIndex=">0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
      <onGameEnter menuIndex=">0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
      <onGameExit  menuIndex=">0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
  </container>
  
    <container shape="curve" curvePosition="right" curveMode="line" x="1855" y="0" width="4" height="1080" curveDepth="150" backgroundColor="FFA500" alpha="0" layer="5">
      <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
      <onMenuEnter menuIndex=">0"><set duration=".00001"><animate type="alpha" to="1"/></set></onMenuEnter>
      <onMenuExit  menuIndex=">0"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
      <onGameEnter menuIndex=">0"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
      <onGameExit  menuIndex=">0"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
  </container>

	  <!--Menu Wheel-->
    <menu type="custom" imageType="logo" orientation="vertical" width="400" maxHeight="175" scrollTime=".2" scrollAcceleration=".1" minScrollTime="0">
        <itemDefaults spacing="2" y="center" x="950" yOrigin="center" xOrigin="center" fontSize="30" width="260" maxWidth="260" autoFit="true" maxFontSize="30"  layer="5"/>
        <item xOffset="830" angle="39" yOffset="-684" alpha="0">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="770" angle="32" yOffset="-576" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="710" angle="27" yOffset="-466" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="665" angle="20" yOffset="-354" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="630" angle="14" yOffset="-238" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="610" angle="8" yOffset="-120" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="600" yOffset="0" layer="11" fontSize="50" alpha="1" selected="true">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="610" angle="-8" yOffset="120" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="630" angle="-14" yOffset="238" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="665" angle="-20" yOffset="354" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="710" angle="-27" yOffset="466" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="770" angle="-32" yOffset="576" alpha="0.75">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
        <item xOffset="830" angle="-39" yOffset="684" alpha="0">
            <onMenuExit>
                <set duration=".00001">
                    <animate type="alpha" to="0"/>
                </set>
            </onMenuExit>
        </item>
    </menu>
	 
	   <!--Background Fanart-->
	 <menu menuIndex="0"  type="custom" imageType="fanart" alpha="0" scrollTime=".2" scrollAcceleration=".1" minScrollTime="0" orientation="horizontal">
		<itemDefaults alpha="0" x="center" y="center" xOrigin="center" yOrigin="center" width="1920" height="1080" layer="1"/>

		<item width="2048" height="1152" alpha="0"/>
		<item selected="true" alpha="1">
			<onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
            <onMenuEnter menuIndex="0"><set duration=".1"><animate type="alpha" to="1"/></set></onMenuEnter>
            <onMenuExit  menuIndex="0"><set duration="0"><animate type="alpha" to="0"/></set></onMenuExit>
		</item>
		<item width="2048" height="1152" alpha="0"/>
	</menu>
	
	 	
	<menu menuIndex="1" type="custom" imageType="fanart" alpha="0" scrollTime=".2" scrollAcceleration=".1" minScrollTime="0" orientation="vertical">
    <itemDefaults alpha="0" x="center" y="center" xOrigin="center" yOrigin="center" width="1920" height="1080" layer="1"/>

    <item width="2048" height="1152" alpha="0"/>
    <item selected="true" alpha="1">
        <onExit><set duration=".00001"><animate type="alpha" to="0"/></set></onExit>
        <onMenuEnter menuIndex="1"><set duration=".1"><animate type="alpha" to="1"/></set></onMenuEnter>
        <onMenuExit  menuIndex="1"><set duration=".00001"><animate type="alpha" to="0"/></set></onMenuExit>
        <onGameEnter menuIndex="1"><set duration=".00001"><animate type="alpha" to="0"/></set></onGameEnter>
        <onGameExit  menuIndex="1"><set duration=".00001"><animate type="alpha" to="1"/></set></onGameExit>
    </item>
    <item width="2048" height="1152" alpha="0"/>
</menu>


</layout>
)xml";

// Splash layout XML (used as fallback when splash.xml is not found on disk)
static const char kSplashXml[] = R"xml(<?xml version="1.0" encoding="UTF-8"?>
<layout width="1920" height="1080" font="fonts/Arial.ttf" loadFontSize="30" fontColor="808080"></layout>
)xml";

inline std::string getLayoutXml()
{
    return std::string(kLayoutXmlPart1) + std::string(kLayoutXmlPart2);
}

inline std::string getSplashXml()
{
    return std::string(kSplashXml);
}

} // namespace DefaultLayout
