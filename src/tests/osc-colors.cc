#include <cstdio>
#include <string>

#include "src/statesync/completeterminal.h"
#include "src/statesync/user.h"
#include "src/terminal/parseraction.h"
#include "src/terminal/terminaldisplay.h"
#include "src/terminal/terminalframebuffer.h"

static bool expect_equal( const char* name, const std::string& got, const std::string& expected )
{
  if ( got == expected ) {
    return true;
  }

  fprintf( stderr, "%s: expected", name );
  for ( const unsigned char c : expected ) {
    fprintf( stderr, " %02x", c );
  }
  fprintf( stderr, ", got" );
  for ( const unsigned char c : got ) {
    fprintf( stderr, " %02x", c );
  }
  fprintf( stderr, "\n" );
  return false;
}

static bool expect_true( const char* name, bool value )
{
  if ( value ) {
    return true;
  }

  fprintf( stderr, "%s: condition failed\n", name );
  return false;
}

static bool test_osc_10_11_queries( void )
{
  Terminal::Complete term( 80, 24 );
  Parser::TerminalColors colors( "rgb:ffff/eeee/dddd", "rgb:0000/1111/2222" );
  term.act( colors );

  bool pass = true;
  pass &= expect_equal( "osc 10 st",
                        term.act( "\033]10;?\033\\" ),
                        "\033]10;rgb:ffff/eeee/dddd\033\\" );
  pass &= expect_equal( "osc 11 bel",
                        term.act( "\033]11;?\007" ),
                        "\033]11;rgb:0000/1111/2222\007" );
  pass &= expect_equal( "osc 10 malformed", term.act( "\033]10;?junk\033\\" ), "" );
  return pass;
}

static bool test_osc_4_queries_and_sets( void )
{
  Terminal::Complete term( 80, 24 );
  Parser::TerminalColors::IndexedColors indexed_colors;
  indexed_colors.push_back( Parser::TerminalColors::IndexedColor { 0, "rgb:0000/0000/0000" } );
  indexed_colors.push_back( Parser::TerminalColors::IndexedColor { 1, "rgb:1111/1111/1111" } );
  term.act( Parser::TerminalColors( "", "", indexed_colors ) );

  bool pass = true;
  pass &= expect_equal( "osc 4 cached query",
                        term.act( "\033]4;1;?\033\\" ),
                        "\033]4;1;rgb:1111/1111/1111\033\\" );
  pass &= expect_equal( "osc 4 missing query", term.act( "\033]4;2;?\033\\" ), "" );
  pass &= expect_equal( "osc 4 set", term.act( "\033]4;1;rgb:aaaa/bbbb/cccc\033\\" ), "" );
  pass &= expect_equal( "osc 4 override query",
                        term.act( "\033]4;1;?\007" ),
                        "\033]4;1;rgb:aaaa/bbbb/cccc\007" );

  std::string color;
  pass &= expect_true( "osc 4 palette stored", term.get_fb().get_palette_color( 1, color ) );
  pass &= expect_equal( "osc 4 palette value", color, "rgb:aaaa/bbbb/cccc" );

  pass &= expect_equal( "osc 4 multi set",
                        term.act( "\033]4;2;rgb:2222/2222/2222;3;rgb:3333/3333/3333\033\\" ),
                        "" );
  pass &= expect_true( "osc 4 multi set index 2", term.get_fb().get_palette_color( 2, color ) );
  pass &= expect_true( "osc 4 multi set index 3", term.get_fb().get_palette_color( 3, color ) );

  pass &= expect_equal( "osc 104 reset one", term.act( "\033]104;1\033\\" ), "" );
  pass &= expect_true( "osc 104 removed override", !term.get_fb().get_palette_color( 1, color ) );
  pass &= expect_equal( "osc 4 query after reset",
                        term.act( "\033]4;1;?\033\\" ),
                        "\033]4;1;rgb:1111/1111/1111\033\\" );

  pass &= expect_equal( "osc 104 reset all", term.act( "\033]104\033\\" ), "" );
  pass &= expect_true( "osc 104 all removed", term.get_fb().get_palette().empty() );

  return pass;
}

static bool test_display_palette_diff( void )
{
  Terminal::Display display( false );
  Terminal::Framebuffer old_frame( 80, 24 );
  Terminal::Framebuffer new_frame( old_frame );
  new_frame.set_palette_color( 5, "rgb:5555/5555/5555" );

  bool pass = true;
  pass &= expect_equal( "display palette set",
                        display.new_frame( true, old_frame, new_frame ),
                        "\033]4;5;rgb:5555/5555/5555\007" );

  Terminal::Framebuffer reset_frame( new_frame );
  reset_frame.reset_palette_color( 5 );
  pass &= expect_equal( "display palette reset",
                        display.new_frame( true, new_frame, reset_frame ),
                        "\033]104;5\007" );

  return pass;
}

static bool test_osc_regressions( void )
{
  Terminal::Complete term( 80, 24 );
  bool pass = true;

  pass &= expect_equal( "osc 0 title", term.act( "\033]0;Title\033\\" ), "" );
  std::wstring title( L"Title" );
  pass &= expect_true( "osc 0 title stored",
                       term.get_fb().get_window_title()
                         == Terminal::Framebuffer::title_type( title.begin(), title.end() ) );

  pass &= expect_equal( "osc 52 clipboard", term.act( "\033]52;c;SGVsbG8=\033\\" ), "" );
  std::wstring clipboard( L"SGVsbG8=" );
  pass &= expect_true( "osc 52 clipboard stored",
                       term.get_fb().get_clipboard()
                         == Terminal::Framebuffer::title_type( clipboard.begin(), clipboard.end() ) );

  pass &= expect_equal( "osc 8 hyperlink", term.act( "\033]8;;https://example.com\033\\" ), "" );
  pass &= expect_true( "osc 8 hyperlink stored", !term.get_fb().ds.get_hyperlink().empty() );
  pass &= expect_equal( "osc 8 hyperlink clear", term.act( "\033]8;;\033\\" ), "" );
  pass &= expect_true( "osc 8 hyperlink cleared", term.get_fb().ds.get_hyperlink().empty() );

  return pass;
}

static bool test_userstream_roundtrip( void )
{
  Parser::TerminalColors::IndexedColors indexed_colors;
  indexed_colors.push_back( Parser::TerminalColors::IndexedColor { 0, "rgb:0000/0000/0000" } );
  indexed_colors.push_back( Parser::TerminalColors::IndexedColor { 7, "rgb:7777/7777/7777" } );

  Network::UserStream output;
  output.push_back( Parser::TerminalColors( "rgb:ffff/ffff/ffff", "rgb:0000/0000/0000", indexed_colors ) );

  Network::UserStream input;
  input.apply_string( output.init_diff() );

  return expect_true( "userstream terminal colors roundtrip", input == output );
}

int main( void )
{
  bool pass = true;
  pass &= test_osc_10_11_queries();
  pass &= test_osc_4_queries_and_sets();
  pass &= test_display_palette_diff();
  pass &= test_osc_regressions();
  pass &= test_userstream_roundtrip();

  return pass ? 0 : 1;
}
