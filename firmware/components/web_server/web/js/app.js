//---------------------------------------------------------------------
//
// QR Code Generator for JavaScript
//
// Copyright (c) 2009 Kazuhiko Arase
//
// URL: http://www.d-project.com/
//
// Licensed under the MIT license:
//  http://www.opensource.org/licenses/mit-license.php
//
// The word 'QR Code' is registered trademark of
// DENSO WAVE INCORPORATED
//  http://www.denso-wave.com/qrcode/faqpatent-e.html
//
//---------------------------------------------------------------------

var qrcode = function() {

  //---------------------------------------------------------------------
  // qrcode
  //---------------------------------------------------------------------

  /**
   * qrcode
   * @param typeNumber 1 to 40
   * @param errorCorrectionLevel 'L','M','Q','H'
   */
  var qrcode = function(typeNumber, errorCorrectionLevel) {

    var PAD0 = 0xEC;
    var PAD1 = 0x11;

    var _typeNumber = typeNumber;
    var _errorCorrectionLevel = QRErrorCorrectionLevel[errorCorrectionLevel];
    var _modules = null;
    var _moduleCount = 0;
    var _dataCache = null;
    var _dataList = [];

    var _this = {};

    var makeImpl = function(test, maskPattern) {

      _moduleCount = _typeNumber * 4 + 17;
      _modules = function(moduleCount) {
        var modules = new Array(moduleCount);
        for (var row = 0; row < moduleCount; row += 1) {
          modules[row] = new Array(moduleCount);
          for (var col = 0; col < moduleCount; col += 1) {
            modules[row][col] = null;
          }
        }
        return modules;
      }(_moduleCount);

      setupPositionProbePattern(0, 0);
      setupPositionProbePattern(_moduleCount - 7, 0);
      setupPositionProbePattern(0, _moduleCount - 7);
      setupPositionAdjustPattern();
      setupTimingPattern();
      setupTypeInfo(test, maskPattern);

      if (_typeNumber >= 7) {
        setupTypeNumber(test);
      }

      if (_dataCache == null) {
        _dataCache = createData(_typeNumber, _errorCorrectionLevel, _dataList);
      }

      mapData(_dataCache, maskPattern);
    };

    var setupPositionProbePattern = function(row, col) {

      for (var r = -1; r <= 7; r += 1) {

        if (row + r <= -1 || _moduleCount <= row + r) continue;

        for (var c = -1; c <= 7; c += 1) {

          if (col + c <= -1 || _moduleCount <= col + c) continue;

          if ( (0 <= r && r <= 6 && (c == 0 || c == 6) )
              || (0 <= c && c <= 6 && (r == 0 || r == 6) )
              || (2 <= r && r <= 4 && 2 <= c && c <= 4) ) {
            _modules[row + r][col + c] = true;
          } else {
            _modules[row + r][col + c] = false;
          }
        }
      }
    };

    var getBestMaskPattern = function() {

      var minLostPoint = 0;
      var pattern = 0;

      for (var i = 0; i < 8; i += 1) {

        makeImpl(true, i);

        var lostPoint = QRUtil.getLostPoint(_this);

        if (i == 0 || minLostPoint > lostPoint) {
          minLostPoint = lostPoint;
          pattern = i;
        }
      }

      return pattern;
    };

    var setupTimingPattern = function() {

      for (var r = 8; r < _moduleCount - 8; r += 1) {
        if (_modules[r][6] != null) {
          continue;
        }
        _modules[r][6] = (r % 2 == 0);
      }

      for (var c = 8; c < _moduleCount - 8; c += 1) {
        if (_modules[6][c] != null) {
          continue;
        }
        _modules[6][c] = (c % 2 == 0);
      }
    };

    var setupPositionAdjustPattern = function() {

      var pos = QRUtil.getPatternPosition(_typeNumber);

      for (var i = 0; i < pos.length; i += 1) {

        for (var j = 0; j < pos.length; j += 1) {

          var row = pos[i];
          var col = pos[j];

          if (_modules[row][col] != null) {
            continue;
          }

          for (var r = -2; r <= 2; r += 1) {

            for (var c = -2; c <= 2; c += 1) {

              if (r == -2 || r == 2 || c == -2 || c == 2
                  || (r == 0 && c == 0) ) {
                _modules[row + r][col + c] = true;
              } else {
                _modules[row + r][col + c] = false;
              }
            }
          }
        }
      }
    };

    var setupTypeNumber = function(test) {

      var bits = QRUtil.getBCHTypeNumber(_typeNumber);

      for (var i = 0; i < 18; i += 1) {
        var mod = (!test && ( (bits >> i) & 1) == 1);
        _modules[Math.floor(i / 3)][i % 3 + _moduleCount - 8 - 3] = mod;
      }

      for (var i = 0; i < 18; i += 1) {
        var mod = (!test && ( (bits >> i) & 1) == 1);
        _modules[i % 3 + _moduleCount - 8 - 3][Math.floor(i / 3)] = mod;
      }
    };

    var setupTypeInfo = function(test, maskPattern) {

      var data = (_errorCorrectionLevel << 3) | maskPattern;
      var bits = QRUtil.getBCHTypeInfo(data);

      // vertical
      for (var i = 0; i < 15; i += 1) {

        var mod = (!test && ( (bits >> i) & 1) == 1);

        if (i < 6) {
          _modules[i][8] = mod;
        } else if (i < 8) {
          _modules[i + 1][8] = mod;
        } else {
          _modules[_moduleCount - 15 + i][8] = mod;
        }
      }

      // horizontal
      for (var i = 0; i < 15; i += 1) {

        var mod = (!test && ( (bits >> i) & 1) == 1);

        if (i < 8) {
          _modules[8][_moduleCount - i - 1] = mod;
        } else if (i < 9) {
          _modules[8][15 - i - 1 + 1] = mod;
        } else {
          _modules[8][15 - i - 1] = mod;
        }
      }

      // fixed module
      _modules[_moduleCount - 8][8] = (!test);
    };

    var mapData = function(data, maskPattern) {

      var inc = -1;
      var row = _moduleCount - 1;
      var bitIndex = 7;
      var byteIndex = 0;
      var maskFunc = QRUtil.getMaskFunction(maskPattern);

      for (var col = _moduleCount - 1; col > 0; col -= 2) {

        if (col == 6) col -= 1;

        while (true) {

          for (var c = 0; c < 2; c += 1) {

            if (_modules[row][col - c] == null) {

              var dark = false;

              if (byteIndex < data.length) {
                dark = ( ( (data[byteIndex] >>> bitIndex) & 1) == 1);
              }

              var mask = maskFunc(row, col - c);

              if (mask) {
                dark = !dark;
              }

              _modules[row][col - c] = dark;
              bitIndex -= 1;

              if (bitIndex == -1) {
                byteIndex += 1;
                bitIndex = 7;
              }
            }
          }

          row += inc;

          if (row < 0 || _moduleCount <= row) {
            row -= inc;
            inc = -inc;
            break;
          }
        }
      }
    };

    var createBytes = function(buffer, rsBlocks) {

      var offset = 0;

      var maxDcCount = 0;
      var maxEcCount = 0;

      var dcdata = new Array(rsBlocks.length);
      var ecdata = new Array(rsBlocks.length);

      for (var r = 0; r < rsBlocks.length; r += 1) {

        var dcCount = rsBlocks[r].dataCount;
        var ecCount = rsBlocks[r].totalCount - dcCount;

        maxDcCount = Math.max(maxDcCount, dcCount);
        maxEcCount = Math.max(maxEcCount, ecCount);

        dcdata[r] = new Array(dcCount);

        for (var i = 0; i < dcdata[r].length; i += 1) {
          dcdata[r][i] = 0xff & buffer.getBuffer()[i + offset];
        }
        offset += dcCount;

        var rsPoly = QRUtil.getErrorCorrectPolynomial(ecCount);
        var rawPoly = qrPolynomial(dcdata[r], rsPoly.getLength() - 1);

        var modPoly = rawPoly.mod(rsPoly);
        ecdata[r] = new Array(rsPoly.getLength() - 1);
        for (var i = 0; i < ecdata[r].length; i += 1) {
          var modIndex = i + modPoly.getLength() - ecdata[r].length;
          ecdata[r][i] = (modIndex >= 0)? modPoly.getAt(modIndex) : 0;
        }
      }

      var totalCodeCount = 0;
      for (var i = 0; i < rsBlocks.length; i += 1) {
        totalCodeCount += rsBlocks[i].totalCount;
      }

      var data = new Array(totalCodeCount);
      var index = 0;

      for (var i = 0; i < maxDcCount; i += 1) {
        for (var r = 0; r < rsBlocks.length; r += 1) {
          if (i < dcdata[r].length) {
            data[index] = dcdata[r][i];
            index += 1;
          }
        }
      }

      for (var i = 0; i < maxEcCount; i += 1) {
        for (var r = 0; r < rsBlocks.length; r += 1) {
          if (i < ecdata[r].length) {
            data[index] = ecdata[r][i];
            index += 1;
          }
        }
      }

      return data;
    };

    var createData = function(typeNumber, errorCorrectionLevel, dataList) {

      var rsBlocks = QRRSBlock.getRSBlocks(typeNumber, errorCorrectionLevel);

      var buffer = qrBitBuffer();

      for (var i = 0; i < dataList.length; i += 1) {
        var data = dataList[i];
        buffer.put(data.getMode(), 4);
        buffer.put(data.getLength(), QRUtil.getLengthInBits(data.getMode(), typeNumber) );
        data.write(buffer);
      }

      // calc num max data.
      var totalDataCount = 0;
      for (var i = 0; i < rsBlocks.length; i += 1) {
        totalDataCount += rsBlocks[i].dataCount;
      }

      if (buffer.getLengthInBits() > totalDataCount * 8) {
        throw 'code length overflow. ('
          + buffer.getLengthInBits()
          + '>'
          + totalDataCount * 8
          + ')';
      }

      // end code
      if (buffer.getLengthInBits() + 4 <= totalDataCount * 8) {
        buffer.put(0, 4);
      }

      // padding
      while (buffer.getLengthInBits() % 8 != 0) {
        buffer.putBit(false);
      }

      // padding
      while (true) {

        if (buffer.getLengthInBits() >= totalDataCount * 8) {
          break;
        }
        buffer.put(PAD0, 8);

        if (buffer.getLengthInBits() >= totalDataCount * 8) {
          break;
        }
        buffer.put(PAD1, 8);
      }

      return createBytes(buffer, rsBlocks);
    };

    _this.addData = function(data, mode) {

      mode = mode || 'Byte';

      var newData = null;

      switch(mode) {
      case 'Numeric' :
        newData = qrNumber(data);
        break;
      case 'Alphanumeric' :
        newData = qrAlphaNum(data);
        break;
      case 'Byte' :
        newData = qr8BitByte(data);
        break;
      case 'Kanji' :
        newData = qrKanji(data);
        break;
      default :
        throw 'mode:' + mode;
      }

      _dataList.push(newData);
      _dataCache = null;
    };

    _this.isDark = function(row, col) {
      if (row < 0 || _moduleCount <= row || col < 0 || _moduleCount <= col) {
        throw row + ',' + col;
      }
      return _modules[row][col];
    };

    _this.getModuleCount = function() {
      return _moduleCount;
    };

    _this.make = function() {
      if (_typeNumber < 1) {
        var typeNumber = 1;

        for (; typeNumber < 40; typeNumber++) {
          var rsBlocks = QRRSBlock.getRSBlocks(typeNumber, _errorCorrectionLevel);
          var buffer = qrBitBuffer();

          for (var i = 0; i < _dataList.length; i++) {
            var data = _dataList[i];
            buffer.put(data.getMode(), 4);
            buffer.put(data.getLength(), QRUtil.getLengthInBits(data.getMode(), typeNumber) );
            data.write(buffer);
          }

          var totalDataCount = 0;
          for (var i = 0; i < rsBlocks.length; i++) {
            totalDataCount += rsBlocks[i].dataCount;
          }

          if (buffer.getLengthInBits() <= totalDataCount * 8) {
            break;
          }
        }

        _typeNumber = typeNumber;
      }

      makeImpl(false, getBestMaskPattern() );
    };

    _this.createTableTag = function(cellSize, margin) {

      cellSize = cellSize || 2;
      margin = (typeof margin == 'undefined')? cellSize * 4 : margin;

      var qrHtml = '';

      qrHtml += '<table style="';
      qrHtml += ' border-width: 0px; border-style: none;';
      qrHtml += ' border-collapse: collapse;';
      qrHtml += ' padding: 0px; margin: ' + margin + 'px;';
      qrHtml += '">';
      qrHtml += '<tbody>';

      for (var r = 0; r < _this.getModuleCount(); r += 1) {

        qrHtml += '<tr>';

        for (var c = 0; c < _this.getModuleCount(); c += 1) {
          qrHtml += '<td style="';
          qrHtml += ' border-width: 0px; border-style: none;';
          qrHtml += ' border-collapse: collapse;';
          qrHtml += ' padding: 0px; margin: 0px;';
          qrHtml += ' width: ' + cellSize + 'px;';
          qrHtml += ' height: ' + cellSize + 'px;';
          qrHtml += ' background-color: ';
          qrHtml += _this.isDark(r, c)? '#000000' : '#ffffff';
          qrHtml += ';';
          qrHtml += '"/>';
        }

        qrHtml += '</tr>';
      }

      qrHtml += '</tbody>';
      qrHtml += '</table>';

      return qrHtml;
    };

    _this.createSvgTag = function(cellSize, margin, alt, title) {

      var opts = {};
      if (typeof arguments[0] == 'object') {
        // Called by options.
        opts = arguments[0];
        // overwrite cellSize and margin.
        cellSize = opts.cellSize;
        margin = opts.margin;
        alt = opts.alt;
        title = opts.title;
      }

      cellSize = cellSize || 2;
      margin = (typeof margin == 'undefined')? cellSize * 4 : margin;

      // Compose alt property surrogate
      alt = (typeof alt === 'string') ? {text: alt} : alt || {};
      alt.text = alt.text || null;
      alt.id = (alt.text) ? alt.id || 'qrcode-description' : null;

      // Compose title property surrogate
      title = (typeof title === 'string') ? {text: title} : title || {};
      title.text = title.text || null;
      title.id = (title.text) ? title.id || 'qrcode-title' : null;

      var size = _this.getModuleCount() * cellSize + margin * 2;
      var c, mc, r, mr, qrSvg='', rect;

      rect = 'l' + cellSize + ',0 0,' + cellSize +
        ' -' + cellSize + ',0 0,-' + cellSize + 'z ';

      qrSvg += '<svg version="1.1" xmlns="http://www.w3.org/2000/svg"';
      qrSvg += !opts.scalable ? ' width="' + size + 'px" height="' + size + 'px"' : '';
      qrSvg += ' viewBox="0 0 ' + size + ' ' + size + '" ';
      qrSvg += ' preserveAspectRatio="xMinYMin meet"';
      qrSvg += (title.text || alt.text) ? ' role="img" aria-labelledby="' +
          escapeXml([title.id, alt.id].join(' ').trim() ) + '"' : '';
      qrSvg += '>';
      qrSvg += (title.text) ? '<title id="' + escapeXml(title.id) + '">' +
          escapeXml(title.text) + '</title>' : '';
      qrSvg += (alt.text) ? '<description id="' + escapeXml(alt.id) + '">' +
          escapeXml(alt.text) + '</description>' : '';
      qrSvg += '<rect width="100%" height="100%" fill="white" cx="0" cy="0"/>';
      qrSvg += '<path d="';

      for (r = 0; r < _this.getModuleCount(); r += 1) {
        mr = r * cellSize + margin;
        for (c = 0; c < _this.getModuleCount(); c += 1) {
          if (_this.isDark(r, c) ) {
            mc = c*cellSize+margin;
            qrSvg += 'M' + mc + ',' + mr + rect;
          }
        }
      }

      qrSvg += '" stroke="transparent" fill="black"/>';
      qrSvg += '</svg>';

      return qrSvg;
    };

    _this.createDataURL = function(cellSize, margin) {

      cellSize = cellSize || 2;
      margin = (typeof margin == 'undefined')? cellSize * 4 : margin;

      var size = _this.getModuleCount() * cellSize + margin * 2;
      var min = margin;
      var max = size - margin;

      return createDataURL(size, size, function(x, y) {
        if (min <= x && x < max && min <= y && y < max) {
          var c = Math.floor( (x - min) / cellSize);
          var r = Math.floor( (y - min) / cellSize);
          return _this.isDark(r, c)? 0 : 1;
        } else {
          return 1;
        }
      } );
    };

    _this.createImgTag = function(cellSize, margin, alt) {

      cellSize = cellSize || 2;
      margin = (typeof margin == 'undefined')? cellSize * 4 : margin;

      var size = _this.getModuleCount() * cellSize + margin * 2;

      var img = '';
      img += '<img';
      img += '\u0020src="';
      img += _this.createDataURL(cellSize, margin);
      img += '"';
      img += '\u0020width="';
      img += size;
      img += '"';
      img += '\u0020height="';
      img += size;
      img += '"';
      if (alt) {
        img += '\u0020alt="';
        img += escapeXml(alt);
        img += '"';
      }
      img += '/>';

      return img;
    };

    var escapeXml = function(s) {
      var escaped = '';
      for (var i = 0; i < s.length; i += 1) {
        var c = s.charAt(i);
        switch(c) {
        case '<': escaped += '&lt;'; break;
        case '>': escaped += '&gt;'; break;
        case '&': escaped += '&amp;'; break;
        case '"': escaped += '&quot;'; break;
        default : escaped += c; break;
        }
      }
      return escaped;
    };

    var _createHalfASCII = function(margin) {
      var cellSize = 1;
      margin = (typeof margin == 'undefined')? cellSize * 2 : margin;

      var size = _this.getModuleCount() * cellSize + margin * 2;
      var min = margin;
      var max = size - margin;

      var y, x, r1, r2, p;

      var blocks = {
        '██': '█',
        '█ ': '▀',
        ' █': '▄',
        '  ': ' '
      };

      var blocksLastLineNoMargin = {
        '██': '▀',
        '█ ': '▀',
        ' █': ' ',
        '  ': ' '
      };

      var ascii = '';
      for (y = 0; y < size; y += 2) {
        r1 = Math.floor((y - min) / cellSize);
        r2 = Math.floor((y + 1 - min) / cellSize);
        for (x = 0; x < size; x += 1) {
          p = '█';

          if (min <= x && x < max && min <= y && y < max && _this.isDark(r1, Math.floor((x - min) / cellSize))) {
            p = ' ';
          }

          if (min <= x && x < max && min <= y+1 && y+1 < max && _this.isDark(r2, Math.floor((x - min) / cellSize))) {
            p += ' ';
          }
          else {
            p += '█';
          }

          // Output 2 characters per pixel, to create full square. 1 character per pixels gives only half width of square.
          ascii += (margin < 1 && y+1 >= max) ? blocksLastLineNoMargin[p] : blocks[p];
        }

        ascii += '\n';
      }

      if (size % 2 && margin > 0) {
        return ascii.substring(0, ascii.length - size - 1) + Array(size+1).join('▀');
      }

      return ascii.substring(0, ascii.length-1);
    };

    _this.createASCII = function(cellSize, margin) {
      cellSize = cellSize || 1;

      if (cellSize < 2) {
        return _createHalfASCII(margin);
      }

      cellSize -= 1;
      margin = (typeof margin == 'undefined')? cellSize * 2 : margin;

      var size = _this.getModuleCount() * cellSize + margin * 2;
      var min = margin;
      var max = size - margin;

      var y, x, r, p;

      var white = Array(cellSize+1).join('██');
      var black = Array(cellSize+1).join('  ');

      var ascii = '';
      var line = '';
      for (y = 0; y < size; y += 1) {
        r = Math.floor( (y - min) / cellSize);
        line = '';
        for (x = 0; x < size; x += 1) {
          p = 1;

          if (min <= x && x < max && min <= y && y < max && _this.isDark(r, Math.floor((x - min) / cellSize))) {
            p = 0;
          }

          // Output 2 characters per pixel, to create full square. 1 character per pixels gives only half width of square.
          line += p ? white : black;
        }

        for (r = 0; r < cellSize; r += 1) {
          ascii += line + '\n';
        }
      }

      return ascii.substring(0, ascii.length-1);
    };

    _this.renderTo2dContext = function(context, cellSize) {
      cellSize = cellSize || 2;
      var length = _this.getModuleCount();
      for (var row = 0; row < length; row++) {
        for (var col = 0; col < length; col++) {
          context.fillStyle = _this.isDark(row, col) ? 'black' : 'white';
          context.fillRect(col * cellSize, row * cellSize, cellSize, cellSize);
        }
      }
    }

    return _this;
  };

  //---------------------------------------------------------------------
  // qrcode.stringToBytes
  //---------------------------------------------------------------------

  qrcode.stringToBytesFuncs = {
    'default' : function(s) {
      var bytes = [];
      for (var i = 0; i < s.length; i += 1) {
        var c = s.charCodeAt(i);
        bytes.push(c & 0xff);
      }
      return bytes;
    }
  };

  qrcode.stringToBytes = qrcode.stringToBytesFuncs['default'];

  //---------------------------------------------------------------------
  // qrcode.createStringToBytes
  //---------------------------------------------------------------------

  /**
   * @param unicodeData base64 string of byte array.
   * [16bit Unicode],[16bit Bytes], ...
   * @param numChars
   */
  qrcode.createStringToBytes = function(unicodeData, numChars) {

    // create conversion map.

    var unicodeMap = function() {

      var bin = base64DecodeInputStream(unicodeData);
      var read = function() {
        var b = bin.read();
        if (b == -1) throw 'eof';
        return b;
      };

      var count = 0;
      var unicodeMap = {};
      while (true) {
        var b0 = bin.read();
        if (b0 == -1) break;
        var b1 = read();
        var b2 = read();
        var b3 = read();
        var k = String.fromCharCode( (b0 << 8) | b1);
        var v = (b2 << 8) | b3;
        unicodeMap[k] = v;
        count += 1;
      }
      if (count != numChars) {
        throw count + ' != ' + numChars;
      }

      return unicodeMap;
    }();

    var unknownChar = '?'.charCodeAt(0);

    return function(s) {
      var bytes = [];
      for (var i = 0; i < s.length; i += 1) {
        var c = s.charCodeAt(i);
        if (c < 128) {
          bytes.push(c);
        } else {
          var b = unicodeMap[s.charAt(i)];
          if (typeof b == 'number') {
            if ( (b & 0xff) == b) {
              // 1byte
              bytes.push(b);
            } else {
              // 2bytes
              bytes.push(b >>> 8);
              bytes.push(b & 0xff);
            }
          } else {
            bytes.push(unknownChar);
          }
        }
      }
      return bytes;
    };
  };

  //---------------------------------------------------------------------
  // QRMode
  //---------------------------------------------------------------------

  var QRMode = {
    MODE_NUMBER :    1 << 0,
    MODE_ALPHA_NUM : 1 << 1,
    MODE_8BIT_BYTE : 1 << 2,
    MODE_KANJI :     1 << 3
  };

  //---------------------------------------------------------------------
  // QRErrorCorrectionLevel
  //---------------------------------------------------------------------

  var QRErrorCorrectionLevel = {
    L : 1,
    M : 0,
    Q : 3,
    H : 2
  };

  //---------------------------------------------------------------------
  // QRMaskPattern
  //---------------------------------------------------------------------

  var QRMaskPattern = {
    PATTERN000 : 0,
    PATTERN001 : 1,
    PATTERN010 : 2,
    PATTERN011 : 3,
    PATTERN100 : 4,
    PATTERN101 : 5,
    PATTERN110 : 6,
    PATTERN111 : 7
  };

  //---------------------------------------------------------------------
  // QRUtil
  //---------------------------------------------------------------------

  var QRUtil = function() {

    var PATTERN_POSITION_TABLE = [
      [],
      [6, 18],
      [6, 22],
      [6, 26],
      [6, 30],
      [6, 34],
      [6, 22, 38],
      [6, 24, 42],
      [6, 26, 46],
      [6, 28, 50],
      [6, 30, 54],
      [6, 32, 58],
      [6, 34, 62],
      [6, 26, 46, 66],
      [6, 26, 48, 70],
      [6, 26, 50, 74],
      [6, 30, 54, 78],
      [6, 30, 56, 82],
      [6, 30, 58, 86],
      [6, 34, 62, 90],
      [6, 28, 50, 72, 94],
      [6, 26, 50, 74, 98],
      [6, 30, 54, 78, 102],
      [6, 28, 54, 80, 106],
      [6, 32, 58, 84, 110],
      [6, 30, 58, 86, 114],
      [6, 34, 62, 90, 118],
      [6, 26, 50, 74, 98, 122],
      [6, 30, 54, 78, 102, 126],
      [6, 26, 52, 78, 104, 130],
      [6, 30, 56, 82, 108, 134],
      [6, 34, 60, 86, 112, 138],
      [6, 30, 58, 86, 114, 142],
      [6, 34, 62, 90, 118, 146],
      [6, 30, 54, 78, 102, 126, 150],
      [6, 24, 50, 76, 102, 128, 154],
      [6, 28, 54, 80, 106, 132, 158],
      [6, 32, 58, 84, 110, 136, 162],
      [6, 26, 54, 82, 110, 138, 166],
      [6, 30, 58, 86, 114, 142, 170]
    ];
    var G15 = (1 << 10) | (1 << 8) | (1 << 5) | (1 << 4) | (1 << 2) | (1 << 1) | (1 << 0);
    var G18 = (1 << 12) | (1 << 11) | (1 << 10) | (1 << 9) | (1 << 8) | (1 << 5) | (1 << 2) | (1 << 0);
    var G15_MASK = (1 << 14) | (1 << 12) | (1 << 10) | (1 << 4) | (1 << 1);

    var _this = {};

    var getBCHDigit = function(data) {
      var digit = 0;
      while (data != 0) {
        digit += 1;
        data >>>= 1;
      }
      return digit;
    };

    _this.getBCHTypeInfo = function(data) {
      var d = data << 10;
      while (getBCHDigit(d) - getBCHDigit(G15) >= 0) {
        d ^= (G15 << (getBCHDigit(d) - getBCHDigit(G15) ) );
      }
      return ( (data << 10) | d) ^ G15_MASK;
    };

    _this.getBCHTypeNumber = function(data) {
      var d = data << 12;
      while (getBCHDigit(d) - getBCHDigit(G18) >= 0) {
        d ^= (G18 << (getBCHDigit(d) - getBCHDigit(G18) ) );
      }
      return (data << 12) | d;
    };

    _this.getPatternPosition = function(typeNumber) {
      return PATTERN_POSITION_TABLE[typeNumber - 1];
    };

    _this.getMaskFunction = function(maskPattern) {

      switch (maskPattern) {

      case QRMaskPattern.PATTERN000 :
        return function(i, j) { return (i + j) % 2 == 0; };
      case QRMaskPattern.PATTERN001 :
        return function(i, j) { return i % 2 == 0; };
      case QRMaskPattern.PATTERN010 :
        return function(i, j) { return j % 3 == 0; };
      case QRMaskPattern.PATTERN011 :
        return function(i, j) { return (i + j) % 3 == 0; };
      case QRMaskPattern.PATTERN100 :
        return function(i, j) { return (Math.floor(i / 2) + Math.floor(j / 3) ) % 2 == 0; };
      case QRMaskPattern.PATTERN101 :
        return function(i, j) { return (i * j) % 2 + (i * j) % 3 == 0; };
      case QRMaskPattern.PATTERN110 :
        return function(i, j) { return ( (i * j) % 2 + (i * j) % 3) % 2 == 0; };
      case QRMaskPattern.PATTERN111 :
        return function(i, j) { return ( (i * j) % 3 + (i + j) % 2) % 2 == 0; };

      default :
        throw 'bad maskPattern:' + maskPattern;
      }
    };

    _this.getErrorCorrectPolynomial = function(errorCorrectLength) {
      var a = qrPolynomial([1], 0);
      for (var i = 0; i < errorCorrectLength; i += 1) {
        a = a.multiply(qrPolynomial([1, QRMath.gexp(i)], 0) );
      }
      return a;
    };

    _this.getLengthInBits = function(mode, type) {

      if (1 <= type && type < 10) {

        // 1 - 9

        switch(mode) {
        case QRMode.MODE_NUMBER    : return 10;
        case QRMode.MODE_ALPHA_NUM : return 9;
        case QRMode.MODE_8BIT_BYTE : return 8;
        case QRMode.MODE_KANJI     : return 8;
        default :
          throw 'mode:' + mode;
        }

      } else if (type < 27) {

        // 10 - 26

        switch(mode) {
        case QRMode.MODE_NUMBER    : return 12;
        case QRMode.MODE_ALPHA_NUM : return 11;
        case QRMode.MODE_8BIT_BYTE : return 16;
        case QRMode.MODE_KANJI     : return 10;
        default :
          throw 'mode:' + mode;
        }

      } else if (type < 41) {

        // 27 - 40

        switch(mode) {
        case QRMode.MODE_NUMBER    : return 14;
        case QRMode.MODE_ALPHA_NUM : return 13;
        case QRMode.MODE_8BIT_BYTE : return 16;
        case QRMode.MODE_KANJI     : return 12;
        default :
          throw 'mode:' + mode;
        }

      } else {
        throw 'type:' + type;
      }
    };

    _this.getLostPoint = function(qrcode) {

      var moduleCount = qrcode.getModuleCount();

      var lostPoint = 0;

      // LEVEL1

      for (var row = 0; row < moduleCount; row += 1) {
        for (var col = 0; col < moduleCount; col += 1) {

          var sameCount = 0;
          var dark = qrcode.isDark(row, col);

          for (var r = -1; r <= 1; r += 1) {

            if (row + r < 0 || moduleCount <= row + r) {
              continue;
            }

            for (var c = -1; c <= 1; c += 1) {

              if (col + c < 0 || moduleCount <= col + c) {
                continue;
              }

              if (r == 0 && c == 0) {
                continue;
              }

              if (dark == qrcode.isDark(row + r, col + c) ) {
                sameCount += 1;
              }
            }
          }

          if (sameCount > 5) {
            lostPoint += (3 + sameCount - 5);
          }
        }
      };

      // LEVEL2

      for (var row = 0; row < moduleCount - 1; row += 1) {
        for (var col = 0; col < moduleCount - 1; col += 1) {
          var count = 0;
          if (qrcode.isDark(row, col) ) count += 1;
          if (qrcode.isDark(row + 1, col) ) count += 1;
          if (qrcode.isDark(row, col + 1) ) count += 1;
          if (qrcode.isDark(row + 1, col + 1) ) count += 1;
          if (count == 0 || count == 4) {
            lostPoint += 3;
          }
        }
      }

      // LEVEL3

      for (var row = 0; row < moduleCount; row += 1) {
        for (var col = 0; col < moduleCount - 6; col += 1) {
          if (qrcode.isDark(row, col)
              && !qrcode.isDark(row, col + 1)
              &&  qrcode.isDark(row, col + 2)
              &&  qrcode.isDark(row, col + 3)
              &&  qrcode.isDark(row, col + 4)
              && !qrcode.isDark(row, col + 5)
              &&  qrcode.isDark(row, col + 6) ) {
            lostPoint += 40;
          }
        }
      }

      for (var col = 0; col < moduleCount; col += 1) {
        for (var row = 0; row < moduleCount - 6; row += 1) {
          if (qrcode.isDark(row, col)
              && !qrcode.isDark(row + 1, col)
              &&  qrcode.isDark(row + 2, col)
              &&  qrcode.isDark(row + 3, col)
              &&  qrcode.isDark(row + 4, col)
              && !qrcode.isDark(row + 5, col)
              &&  qrcode.isDark(row + 6, col) ) {
            lostPoint += 40;
          }
        }
      }

      // LEVEL4

      var darkCount = 0;

      for (var col = 0; col < moduleCount; col += 1) {
        for (var row = 0; row < moduleCount; row += 1) {
          if (qrcode.isDark(row, col) ) {
            darkCount += 1;
          }
        }
      }

      var ratio = Math.abs(100 * darkCount / moduleCount / moduleCount - 50) / 5;
      lostPoint += ratio * 10;

      return lostPoint;
    };

    return _this;
  }();

  //---------------------------------------------------------------------
  // QRMath
  //---------------------------------------------------------------------

  var QRMath = function() {

    var EXP_TABLE = new Array(256);
    var LOG_TABLE = new Array(256);

    // initialize tables
    for (var i = 0; i < 8; i += 1) {
      EXP_TABLE[i] = 1 << i;
    }
    for (var i = 8; i < 256; i += 1) {
      EXP_TABLE[i] = EXP_TABLE[i - 4]
        ^ EXP_TABLE[i - 5]
        ^ EXP_TABLE[i - 6]
        ^ EXP_TABLE[i - 8];
    }
    for (var i = 0; i < 255; i += 1) {
      LOG_TABLE[EXP_TABLE[i] ] = i;
    }

    var _this = {};

    _this.glog = function(n) {

      if (n < 1) {
        throw 'glog(' + n + ')';
      }

      return LOG_TABLE[n];
    };

    _this.gexp = function(n) {

      while (n < 0) {
        n += 255;
      }

      while (n >= 256) {
        n -= 255;
      }

      return EXP_TABLE[n];
    };

    return _this;
  }();

  //---------------------------------------------------------------------
  // qrPolynomial
  //---------------------------------------------------------------------

  function qrPolynomial(num, shift) {

    if (typeof num.length == 'undefined') {
      throw num.length + '/' + shift;
    }

    var _num = function() {
      var offset = 0;
      while (offset < num.length && num[offset] == 0) {
        offset += 1;
      }
      var _num = new Array(num.length - offset + shift);
      for (var i = 0; i < num.length - offset; i += 1) {
        _num[i] = num[i + offset];
      }
      return _num;
    }();

    var _this = {};

    _this.getAt = function(index) {
      return _num[index];
    };

    _this.getLength = function() {
      return _num.length;
    };

    _this.multiply = function(e) {

      var num = new Array(_this.getLength() + e.getLength() - 1);

      for (var i = 0; i < _this.getLength(); i += 1) {
        for (var j = 0; j < e.getLength(); j += 1) {
          num[i + j] ^= QRMath.gexp(QRMath.glog(_this.getAt(i) ) + QRMath.glog(e.getAt(j) ) );
        }
      }

      return qrPolynomial(num, 0);
    };

    _this.mod = function(e) {

      if (_this.getLength() - e.getLength() < 0) {
        return _this;
      }

      var ratio = QRMath.glog(_this.getAt(0) ) - QRMath.glog(e.getAt(0) );

      var num = new Array(_this.getLength() );
      for (var i = 0; i < _this.getLength(); i += 1) {
        num[i] = _this.getAt(i);
      }

      for (var i = 0; i < e.getLength(); i += 1) {
        num[i] ^= QRMath.gexp(QRMath.glog(e.getAt(i) ) + ratio);
      }

      // recursive call
      return qrPolynomial(num, 0).mod(e);
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // QRRSBlock
  //---------------------------------------------------------------------

  var QRRSBlock = function() {

    var RS_BLOCK_TABLE = [

      // L
      // M
      // Q
      // H

      // 1
      [1, 26, 19],
      [1, 26, 16],
      [1, 26, 13],
      [1, 26, 9],

      // 2
      [1, 44, 34],
      [1, 44, 28],
      [1, 44, 22],
      [1, 44, 16],

      // 3
      [1, 70, 55],
      [1, 70, 44],
      [2, 35, 17],
      [2, 35, 13],

      // 4
      [1, 100, 80],
      [2, 50, 32],
      [2, 50, 24],
      [4, 25, 9],

      // 5
      [1, 134, 108],
      [2, 67, 43],
      [2, 33, 15, 2, 34, 16],
      [2, 33, 11, 2, 34, 12],

      // 6
      [2, 86, 68],
      [4, 43, 27],
      [4, 43, 19],
      [4, 43, 15],

      // 7
      [2, 98, 78],
      [4, 49, 31],
      [2, 32, 14, 4, 33, 15],
      [4, 39, 13, 1, 40, 14],

      // 8
      [2, 121, 97],
      [2, 60, 38, 2, 61, 39],
      [4, 40, 18, 2, 41, 19],
      [4, 40, 14, 2, 41, 15],

      // 9
      [2, 146, 116],
      [3, 58, 36, 2, 59, 37],
      [4, 36, 16, 4, 37, 17],
      [4, 36, 12, 4, 37, 13],

      // 10
      [2, 86, 68, 2, 87, 69],
      [4, 69, 43, 1, 70, 44],
      [6, 43, 19, 2, 44, 20],
      [6, 43, 15, 2, 44, 16],

      // 11
      [4, 101, 81],
      [1, 80, 50, 4, 81, 51],
      [4, 50, 22, 4, 51, 23],
      [3, 36, 12, 8, 37, 13],

      // 12
      [2, 116, 92, 2, 117, 93],
      [6, 58, 36, 2, 59, 37],
      [4, 46, 20, 6, 47, 21],
      [7, 42, 14, 4, 43, 15],

      // 13
      [4, 133, 107],
      [8, 59, 37, 1, 60, 38],
      [8, 44, 20, 4, 45, 21],
      [12, 33, 11, 4, 34, 12],

      // 14
      [3, 145, 115, 1, 146, 116],
      [4, 64, 40, 5, 65, 41],
      [11, 36, 16, 5, 37, 17],
      [11, 36, 12, 5, 37, 13],

      // 15
      [5, 109, 87, 1, 110, 88],
      [5, 65, 41, 5, 66, 42],
      [5, 54, 24, 7, 55, 25],
      [11, 36, 12, 7, 37, 13],

      // 16
      [5, 122, 98, 1, 123, 99],
      [7, 73, 45, 3, 74, 46],
      [15, 43, 19, 2, 44, 20],
      [3, 45, 15, 13, 46, 16],

      // 17
      [1, 135, 107, 5, 136, 108],
      [10, 74, 46, 1, 75, 47],
      [1, 50, 22, 15, 51, 23],
      [2, 42, 14, 17, 43, 15],

      // 18
      [5, 150, 120, 1, 151, 121],
      [9, 69, 43, 4, 70, 44],
      [17, 50, 22, 1, 51, 23],
      [2, 42, 14, 19, 43, 15],

      // 19
      [3, 141, 113, 4, 142, 114],
      [3, 70, 44, 11, 71, 45],
      [17, 47, 21, 4, 48, 22],
      [9, 39, 13, 16, 40, 14],

      // 20
      [3, 135, 107, 5, 136, 108],
      [3, 67, 41, 13, 68, 42],
      [15, 54, 24, 5, 55, 25],
      [15, 43, 15, 10, 44, 16],

      // 21
      [4, 144, 116, 4, 145, 117],
      [17, 68, 42],
      [17, 50, 22, 6, 51, 23],
      [19, 46, 16, 6, 47, 17],

      // 22
      [2, 139, 111, 7, 140, 112],
      [17, 74, 46],
      [7, 54, 24, 16, 55, 25],
      [34, 37, 13],

      // 23
      [4, 151, 121, 5, 152, 122],
      [4, 75, 47, 14, 76, 48],
      [11, 54, 24, 14, 55, 25],
      [16, 45, 15, 14, 46, 16],

      // 24
      [6, 147, 117, 4, 148, 118],
      [6, 73, 45, 14, 74, 46],
      [11, 54, 24, 16, 55, 25],
      [30, 46, 16, 2, 47, 17],

      // 25
      [8, 132, 106, 4, 133, 107],
      [8, 75, 47, 13, 76, 48],
      [7, 54, 24, 22, 55, 25],
      [22, 45, 15, 13, 46, 16],

      // 26
      [10, 142, 114, 2, 143, 115],
      [19, 74, 46, 4, 75, 47],
      [28, 50, 22, 6, 51, 23],
      [33, 46, 16, 4, 47, 17],

      // 27
      [8, 152, 122, 4, 153, 123],
      [22, 73, 45, 3, 74, 46],
      [8, 53, 23, 26, 54, 24],
      [12, 45, 15, 28, 46, 16],

      // 28
      [3, 147, 117, 10, 148, 118],
      [3, 73, 45, 23, 74, 46],
      [4, 54, 24, 31, 55, 25],
      [11, 45, 15, 31, 46, 16],

      // 29
      [7, 146, 116, 7, 147, 117],
      [21, 73, 45, 7, 74, 46],
      [1, 53, 23, 37, 54, 24],
      [19, 45, 15, 26, 46, 16],

      // 30
      [5, 145, 115, 10, 146, 116],
      [19, 75, 47, 10, 76, 48],
      [15, 54, 24, 25, 55, 25],
      [23, 45, 15, 25, 46, 16],

      // 31
      [13, 145, 115, 3, 146, 116],
      [2, 74, 46, 29, 75, 47],
      [42, 54, 24, 1, 55, 25],
      [23, 45, 15, 28, 46, 16],

      // 32
      [17, 145, 115],
      [10, 74, 46, 23, 75, 47],
      [10, 54, 24, 35, 55, 25],
      [19, 45, 15, 35, 46, 16],

      // 33
      [17, 145, 115, 1, 146, 116],
      [14, 74, 46, 21, 75, 47],
      [29, 54, 24, 19, 55, 25],
      [11, 45, 15, 46, 46, 16],

      // 34
      [13, 145, 115, 6, 146, 116],
      [14, 74, 46, 23, 75, 47],
      [44, 54, 24, 7, 55, 25],
      [59, 46, 16, 1, 47, 17],

      // 35
      [12, 151, 121, 7, 152, 122],
      [12, 75, 47, 26, 76, 48],
      [39, 54, 24, 14, 55, 25],
      [22, 45, 15, 41, 46, 16],

      // 36
      [6, 151, 121, 14, 152, 122],
      [6, 75, 47, 34, 76, 48],
      [46, 54, 24, 10, 55, 25],
      [2, 45, 15, 64, 46, 16],

      // 37
      [17, 152, 122, 4, 153, 123],
      [29, 74, 46, 14, 75, 47],
      [49, 54, 24, 10, 55, 25],
      [24, 45, 15, 46, 46, 16],

      // 38
      [4, 152, 122, 18, 153, 123],
      [13, 74, 46, 32, 75, 47],
      [48, 54, 24, 14, 55, 25],
      [42, 45, 15, 32, 46, 16],

      // 39
      [20, 147, 117, 4, 148, 118],
      [40, 75, 47, 7, 76, 48],
      [43, 54, 24, 22, 55, 25],
      [10, 45, 15, 67, 46, 16],

      // 40
      [19, 148, 118, 6, 149, 119],
      [18, 75, 47, 31, 76, 48],
      [34, 54, 24, 34, 55, 25],
      [20, 45, 15, 61, 46, 16]
    ];

    var qrRSBlock = function(totalCount, dataCount) {
      var _this = {};
      _this.totalCount = totalCount;
      _this.dataCount = dataCount;
      return _this;
    };

    var _this = {};

    var getRsBlockTable = function(typeNumber, errorCorrectionLevel) {

      switch(errorCorrectionLevel) {
      case QRErrorCorrectionLevel.L :
        return RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 0];
      case QRErrorCorrectionLevel.M :
        return RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 1];
      case QRErrorCorrectionLevel.Q :
        return RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 2];
      case QRErrorCorrectionLevel.H :
        return RS_BLOCK_TABLE[(typeNumber - 1) * 4 + 3];
      default :
        return undefined;
      }
    };

    _this.getRSBlocks = function(typeNumber, errorCorrectionLevel) {

      var rsBlock = getRsBlockTable(typeNumber, errorCorrectionLevel);

      if (typeof rsBlock == 'undefined') {
        throw 'bad rs block @ typeNumber:' + typeNumber +
            '/errorCorrectionLevel:' + errorCorrectionLevel;
      }

      var length = rsBlock.length / 3;

      var list = [];

      for (var i = 0; i < length; i += 1) {

        var count = rsBlock[i * 3 + 0];
        var totalCount = rsBlock[i * 3 + 1];
        var dataCount = rsBlock[i * 3 + 2];

        for (var j = 0; j < count; j += 1) {
          list.push(qrRSBlock(totalCount, dataCount) );
        }
      }

      return list;
    };

    return _this;
  }();

  //---------------------------------------------------------------------
  // qrBitBuffer
  //---------------------------------------------------------------------

  var qrBitBuffer = function() {

    var _buffer = [];
    var _length = 0;

    var _this = {};

    _this.getBuffer = function() {
      return _buffer;
    };

    _this.getAt = function(index) {
      var bufIndex = Math.floor(index / 8);
      return ( (_buffer[bufIndex] >>> (7 - index % 8) ) & 1) == 1;
    };

    _this.put = function(num, length) {
      for (var i = 0; i < length; i += 1) {
        _this.putBit( ( (num >>> (length - i - 1) ) & 1) == 1);
      }
    };

    _this.getLengthInBits = function() {
      return _length;
    };

    _this.putBit = function(bit) {

      var bufIndex = Math.floor(_length / 8);
      if (_buffer.length <= bufIndex) {
        _buffer.push(0);
      }

      if (bit) {
        _buffer[bufIndex] |= (0x80 >>> (_length % 8) );
      }

      _length += 1;
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // qrNumber
  //---------------------------------------------------------------------

  var qrNumber = function(data) {

    var _mode = QRMode.MODE_NUMBER;
    var _data = data;

    var _this = {};

    _this.getMode = function() {
      return _mode;
    };

    _this.getLength = function(buffer) {
      return _data.length;
    };

    _this.write = function(buffer) {

      var data = _data;

      var i = 0;

      while (i + 2 < data.length) {
        buffer.put(strToNum(data.substring(i, i + 3) ), 10);
        i += 3;
      }

      if (i < data.length) {
        if (data.length - i == 1) {
          buffer.put(strToNum(data.substring(i, i + 1) ), 4);
        } else if (data.length - i == 2) {
          buffer.put(strToNum(data.substring(i, i + 2) ), 7);
        }
      }
    };

    var strToNum = function(s) {
      var num = 0;
      for (var i = 0; i < s.length; i += 1) {
        num = num * 10 + chatToNum(s.charAt(i) );
      }
      return num;
    };

    var chatToNum = function(c) {
      if ('0' <= c && c <= '9') {
        return c.charCodeAt(0) - '0'.charCodeAt(0);
      }
      throw 'illegal char :' + c;
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // qrAlphaNum
  //---------------------------------------------------------------------

  var qrAlphaNum = function(data) {

    var _mode = QRMode.MODE_ALPHA_NUM;
    var _data = data;

    var _this = {};

    _this.getMode = function() {
      return _mode;
    };

    _this.getLength = function(buffer) {
      return _data.length;
    };

    _this.write = function(buffer) {

      var s = _data;

      var i = 0;

      while (i + 1 < s.length) {
        buffer.put(
          getCode(s.charAt(i) ) * 45 +
          getCode(s.charAt(i + 1) ), 11);
        i += 2;
      }

      if (i < s.length) {
        buffer.put(getCode(s.charAt(i) ), 6);
      }
    };

    var getCode = function(c) {

      if ('0' <= c && c <= '9') {
        return c.charCodeAt(0) - '0'.charCodeAt(0);
      } else if ('A' <= c && c <= 'Z') {
        return c.charCodeAt(0) - 'A'.charCodeAt(0) + 10;
      } else {
        switch (c) {
        case ' ' : return 36;
        case '$' : return 37;
        case '%' : return 38;
        case '*' : return 39;
        case '+' : return 40;
        case '-' : return 41;
        case '.' : return 42;
        case '/' : return 43;
        case ':' : return 44;
        default :
          throw 'illegal char :' + c;
        }
      }
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // qr8BitByte
  //---------------------------------------------------------------------

  var qr8BitByte = function(data) {

    var _mode = QRMode.MODE_8BIT_BYTE;
    var _data = data;
    var _bytes = qrcode.stringToBytes(data);

    var _this = {};

    _this.getMode = function() {
      return _mode;
    };

    _this.getLength = function(buffer) {
      return _bytes.length;
    };

    _this.write = function(buffer) {
      for (var i = 0; i < _bytes.length; i += 1) {
        buffer.put(_bytes[i], 8);
      }
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // qrKanji
  //---------------------------------------------------------------------

  var qrKanji = function(data) {

    var _mode = QRMode.MODE_KANJI;
    var _data = data;

    var stringToBytes = qrcode.stringToBytesFuncs['SJIS'];
    if (!stringToBytes) {
      throw 'sjis not supported.';
    }
    !function(c, code) {
      // self test for sjis support.
      var test = stringToBytes(c);
      if (test.length != 2 || ( (test[0] << 8) | test[1]) != code) {
        throw 'sjis not supported.';
      }
    }('\u53cb', 0x9746);

    var _bytes = stringToBytes(data);

    var _this = {};

    _this.getMode = function() {
      return _mode;
    };

    _this.getLength = function(buffer) {
      return ~~(_bytes.length / 2);
    };

    _this.write = function(buffer) {

      var data = _bytes;

      var i = 0;

      while (i + 1 < data.length) {

        var c = ( (0xff & data[i]) << 8) | (0xff & data[i + 1]);

        if (0x8140 <= c && c <= 0x9FFC) {
          c -= 0x8140;
        } else if (0xE040 <= c && c <= 0xEBBF) {
          c -= 0xC140;
        } else {
          throw 'illegal char at ' + (i + 1) + '/' + c;
        }

        c = ( (c >>> 8) & 0xff) * 0xC0 + (c & 0xff);

        buffer.put(c, 13);

        i += 2;
      }

      if (i < data.length) {
        throw 'illegal char at ' + (i + 1);
      }
    };

    return _this;
  };

  //=====================================================================
  // GIF Support etc.
  //

  //---------------------------------------------------------------------
  // byteArrayOutputStream
  //---------------------------------------------------------------------

  var byteArrayOutputStream = function() {

    var _bytes = [];

    var _this = {};

    _this.writeByte = function(b) {
      _bytes.push(b & 0xff);
    };

    _this.writeShort = function(i) {
      _this.writeByte(i);
      _this.writeByte(i >>> 8);
    };

    _this.writeBytes = function(b, off, len) {
      off = off || 0;
      len = len || b.length;
      for (var i = 0; i < len; i += 1) {
        _this.writeByte(b[i + off]);
      }
    };

    _this.writeString = function(s) {
      for (var i = 0; i < s.length; i += 1) {
        _this.writeByte(s.charCodeAt(i) );
      }
    };

    _this.toByteArray = function() {
      return _bytes;
    };

    _this.toString = function() {
      var s = '';
      s += '[';
      for (var i = 0; i < _bytes.length; i += 1) {
        if (i > 0) {
          s += ',';
        }
        s += _bytes[i];
      }
      s += ']';
      return s;
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // base64EncodeOutputStream
  //---------------------------------------------------------------------

  var base64EncodeOutputStream = function() {

    var _buffer = 0;
    var _buflen = 0;
    var _length = 0;
    var _base64 = '';

    var _this = {};

    var writeEncoded = function(b) {
      _base64 += String.fromCharCode(encode(b & 0x3f) );
    };

    var encode = function(n) {
      if (n < 0) {
        // error.
      } else if (n < 26) {
        return 0x41 + n;
      } else if (n < 52) {
        return 0x61 + (n - 26);
      } else if (n < 62) {
        return 0x30 + (n - 52);
      } else if (n == 62) {
        return 0x2b;
      } else if (n == 63) {
        return 0x2f;
      }
      throw 'n:' + n;
    };

    _this.writeByte = function(n) {

      _buffer = (_buffer << 8) | (n & 0xff);
      _buflen += 8;
      _length += 1;

      while (_buflen >= 6) {
        writeEncoded(_buffer >>> (_buflen - 6) );
        _buflen -= 6;
      }
    };

    _this.flush = function() {

      if (_buflen > 0) {
        writeEncoded(_buffer << (6 - _buflen) );
        _buffer = 0;
        _buflen = 0;
      }

      if (_length % 3 != 0) {
        // padding
        var padlen = 3 - _length % 3;
        for (var i = 0; i < padlen; i += 1) {
          _base64 += '=';
        }
      }
    };

    _this.toString = function() {
      return _base64;
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // base64DecodeInputStream
  //---------------------------------------------------------------------

  var base64DecodeInputStream = function(str) {

    var _str = str;
    var _pos = 0;
    var _buffer = 0;
    var _buflen = 0;

    var _this = {};

    _this.read = function() {

      while (_buflen < 8) {

        if (_pos >= _str.length) {
          if (_buflen == 0) {
            return -1;
          }
          throw 'unexpected end of file./' + _buflen;
        }

        var c = _str.charAt(_pos);
        _pos += 1;

        if (c == '=') {
          _buflen = 0;
          return -1;
        } else if (c.match(/^\s$/) ) {
          // ignore if whitespace.
          continue;
        }

        _buffer = (_buffer << 6) | decode(c.charCodeAt(0) );
        _buflen += 6;
      }

      var n = (_buffer >>> (_buflen - 8) ) & 0xff;
      _buflen -= 8;
      return n;
    };

    var decode = function(c) {
      if (0x41 <= c && c <= 0x5a) {
        return c - 0x41;
      } else if (0x61 <= c && c <= 0x7a) {
        return c - 0x61 + 26;
      } else if (0x30 <= c && c <= 0x39) {
        return c - 0x30 + 52;
      } else if (c == 0x2b) {
        return 62;
      } else if (c == 0x2f) {
        return 63;
      } else {
        throw 'c:' + c;
      }
    };

    return _this;
  };

  //---------------------------------------------------------------------
  // gifImage (B/W)
  //---------------------------------------------------------------------

  var gifImage = function(width, height) {

    var _width = width;
    var _height = height;
    var _data = new Array(width * height);

    var _this = {};

    _this.setPixel = function(x, y, pixel) {
      _data[y * _width + x] = pixel;
    };

    _this.write = function(out) {

      //---------------------------------
      // GIF Signature

      out.writeString('GIF87a');

      //---------------------------------
      // Screen Descriptor

      out.writeShort(_width);
      out.writeShort(_height);

      out.writeByte(0x80); // 2bit
      out.writeByte(0);
      out.writeByte(0);

      //---------------------------------
      // Global Color Map

      // black
      out.writeByte(0x00);
      out.writeByte(0x00);
      out.writeByte(0x00);

      // white
      out.writeByte(0xff);
      out.writeByte(0xff);
      out.writeByte(0xff);

      //---------------------------------
      // Image Descriptor

      out.writeString(',');
      out.writeShort(0);
      out.writeShort(0);
      out.writeShort(_width);
      out.writeShort(_height);
      out.writeByte(0);

      //---------------------------------
      // Local Color Map

      //---------------------------------
      // Raster Data

      var lzwMinCodeSize = 2;
      var raster = getLZWRaster(lzwMinCodeSize);

      out.writeByte(lzwMinCodeSize);

      var offset = 0;

      while (raster.length - offset > 255) {
        out.writeByte(255);
        out.writeBytes(raster, offset, 255);
        offset += 255;
      }

      out.writeByte(raster.length - offset);
      out.writeBytes(raster, offset, raster.length - offset);
      out.writeByte(0x00);

      //---------------------------------
      // GIF Terminator
      out.writeString(';');
    };

    var bitOutputStream = function(out) {

      var _out = out;
      var _bitLength = 0;
      var _bitBuffer = 0;

      var _this = {};

      _this.write = function(data, length) {

        if ( (data >>> length) != 0) {
          throw 'length over';
        }

        while (_bitLength + length >= 8) {
          _out.writeByte(0xff & ( (data << _bitLength) | _bitBuffer) );
          length -= (8 - _bitLength);
          data >>>= (8 - _bitLength);
          _bitBuffer = 0;
          _bitLength = 0;
        }

        _bitBuffer = (data << _bitLength) | _bitBuffer;
        _bitLength = _bitLength + length;
      };

      _this.flush = function() {
        if (_bitLength > 0) {
          _out.writeByte(_bitBuffer);
        }
      };

      return _this;
    };

    var getLZWRaster = function(lzwMinCodeSize) {

      var clearCode = 1 << lzwMinCodeSize;
      var endCode = (1 << lzwMinCodeSize) + 1;
      var bitLength = lzwMinCodeSize + 1;

      // Setup LZWTable
      var table = lzwTable();

      for (var i = 0; i < clearCode; i += 1) {
        table.add(String.fromCharCode(i) );
      }
      table.add(String.fromCharCode(clearCode) );
      table.add(String.fromCharCode(endCode) );

      var byteOut = byteArrayOutputStream();
      var bitOut = bitOutputStream(byteOut);

      // clear code
      bitOut.write(clearCode, bitLength);

      var dataIndex = 0;

      var s = String.fromCharCode(_data[dataIndex]);
      dataIndex += 1;

      while (dataIndex < _data.length) {

        var c = String.fromCharCode(_data[dataIndex]);
        dataIndex += 1;

        if (table.contains(s + c) ) {

          s = s + c;

        } else {

          bitOut.write(table.indexOf(s), bitLength);

          if (table.size() < 0xfff) {

            if (table.size() == (1 << bitLength) ) {
              bitLength += 1;
            }

            table.add(s + c);
          }

          s = c;
        }
      }

      bitOut.write(table.indexOf(s), bitLength);

      // end code
      bitOut.write(endCode, bitLength);

      bitOut.flush();

      return byteOut.toByteArray();
    };

    var lzwTable = function() {

      var _map = {};
      var _size = 0;

      var _this = {};

      _this.add = function(key) {
        if (_this.contains(key) ) {
          throw 'dup key:' + key;
        }
        _map[key] = _size;
        _size += 1;
      };

      _this.size = function() {
        return _size;
      };

      _this.indexOf = function(key) {
        return _map[key];
      };

      _this.contains = function(key) {
        return typeof _map[key] != 'undefined';
      };

      return _this;
    };

    return _this;
  };

  var createDataURL = function(width, height, getPixel) {
    var gif = gifImage(width, height);
    for (var y = 0; y < height; y += 1) {
      for (var x = 0; x < width; x += 1) {
        gif.setPixel(x, y, getPixel(x, y) );
      }
    }

    var b = byteArrayOutputStream();
    gif.write(b);

    var base64 = base64EncodeOutputStream();
    var bytes = b.toByteArray();
    for (var i = 0; i < bytes.length; i += 1) {
      base64.writeByte(bytes[i]);
    }
    base64.flush();

    return 'data:image/gif;base64,' + base64;
  };

  //---------------------------------------------------------------------
  // returns qrcode function.

  return qrcode;
}();

// multibyte support
!function() {

  qrcode.stringToBytesFuncs['UTF-8'] = function(s) {
    // http://stackoverflow.com/questions/18729405/how-to-convert-utf8-string-to-byte-array
    function toUTF8Array(str) {
      var utf8 = [];
      for (var i=0; i < str.length; i++) {
        var charcode = str.charCodeAt(i);
        if (charcode < 0x80) utf8.push(charcode);
        else if (charcode < 0x800) {
          utf8.push(0xc0 | (charcode >> 6),
              0x80 | (charcode & 0x3f));
        }
        else if (charcode < 0xd800 || charcode >= 0xe000) {
          utf8.push(0xe0 | (charcode >> 12),
              0x80 | ((charcode>>6) & 0x3f),
              0x80 | (charcode & 0x3f));
        }
        // surrogate pair
        else {
          i++;
          // UTF-16 encodes 0x10000-0x10FFFF by
          // subtracting 0x10000 and splitting the
          // 20 bits of 0x0-0xFFFFF into two halves
          charcode = 0x10000 + (((charcode & 0x3ff)<<10)
            | (str.charCodeAt(i) & 0x3ff));
          utf8.push(0xf0 | (charcode >>18),
              0x80 | ((charcode>>12) & 0x3f),
              0x80 | ((charcode>>6) & 0x3f),
              0x80 | (charcode & 0x3f));
        }
      }
      return utf8;
    }
    return toUTF8Array(s);
  };

}();

(function (factory) {
  if (typeof define === 'function' && define.amd) {
      define([], factory);
  } else if (typeof exports === 'object') {
      module.exports = factory();
  }
}(function () {
    return qrcode;
}));

;
let state = {};
let socket;

// Directe visuele feedback voor dashboard/bedieningsknoppen.
// De echte pompstatus blijft leidend; een optimistische status blijft
// maximaal enkele seconden zichtbaar totdat de pomp hem bevestigt.
const pendingUiState = new Map();
let pendingUiTimer = null;
let selectedNetwork = "";
let toastTimer;
let preferredTemperatureUnit = localStorage.getItem("layzspaTemperatureUnit") || "Celsius";

const minTemp = 20;
const maxTemp = 40;
const circumference = 603;


function isFahrenheit() {
  return preferredTemperatureUnit === "Fahrenheit";
}

function temperatureSymbol() {
  return isFahrenheit() ? "°F" : "°C";
}

function displayTemperature(value) {
  if (value === null || value === undefined || value === "--") return "--";
  const celsius = Number(value);
  if (!Number.isFinite(celsius)) return value;
  const converted = isFahrenheit() ? (celsius * 9 / 5) + 32 : celsius;
  return Number.isInteger(converted) ? String(converted) : converted.toFixed(1);
}

function applyTemperatureUnit() {
  document.documentElement.classList.toggle("fahrenheit-mode", isFahrenheit());
  document.querySelectorAll(".unit").forEach(el => el.innerText = temperatureSymbol());
  const select = document.getElementById("temperatureUnit");
  if (select && select.value !== preferredTemperatureUnit) select.value = preferredTemperatureUnit;
  setText("temperature", displayTemperature(state.temperature ?? "--"));
  setText("tempSmall", displayTemperature(state.temperature ?? "--"));
  setText("target", displayTemperature(state.target ?? "--"));
  setText("targetBig", displayTemperature(state.target ?? "--"));
}

async function loadTemperaturePreference() {
  try {
    const response = await fetch("/api/settings/region", { cache: "no-store" });
    if (!response.ok) return;
    const settings = await response.json();
    preferredTemperatureUnit = localStorage.getItem("layzspaTemperatureUnit") || settings.temperatureUnit || "Celsius";
    applyTemperatureUnit();
    render();
  } catch (_) {}
}

function schedulePendingUiRefresh() {
  if (pendingUiTimer) clearTimeout(pendingUiTimer);
  pendingUiTimer = null;
  if (!pendingUiState.size) return;

  let nextExpiry = Infinity;
  for (const pending of pendingUiState.values()) {
    nextExpiry = Math.min(nextExpiry, pending.expires);
  }

  pendingUiTimer = setTimeout(() => {
    pendingUiTimer = null;
    if (socket && socket.readyState === WebSocket.OPEN) {
      socket.send("state:get");
    } else {
      pollLiveStateOnce();
    }
  }, Math.max(50, nextExpiry - Date.now() + 25));
}

function setOptimisticField(field, value) {
  pendingUiState.set(field, { value, expires: Date.now() + 7000 });
  state = { ...state, [field]: value };
  if (field === "heater" && value === false) state.heaterActive = false;
  render();
  schedulePendingUiRefresh();
}

function applyOptimisticCommand(command) {
  const toggles = {
    "toggle:power": "power",
    "toggle:filter": "filter",
    "toggle:bubbles": "bubbles",
    "toggle:jets": "jets",
    "toggle:heater": "heater",
    "press:lock": "locked",
    "toggle:unit": "fahrenheit",
    "press:unit": "fahrenheit",
    "press:timer": "timerActive",
    "toggle:godmode": "godMode",
    "toggle:fullpower": "fullPower"
  };

  const field = toggles[command];
  if (!field || typeof state[field] !== "boolean") return;
  setOptimisticField(field, !state[field]);
}

function applyLiveState(nextState) {
  const confirmed = nextState || {};
  const merged = { ...confirmed };
  const now = Date.now();

  for (const [field, pending] of pendingUiState.entries()) {
    if (now >= pending.expires || confirmed[field] === pending.value) {
      pendingUiState.delete(field);
    } else {
      merged[field] = pending.value;
      if (field === "heater" && pending.value === false) merged.heaterActive = false;
    }
  }

  state = merged;
  schedulePendingUiRefresh();

  if (typeof state.fahrenheit === "boolean") {
    const unitFromSpa = state.fahrenheit ? "Fahrenheit" : "Celsius";
    if (preferredTemperatureUnit !== unitFromSpa) {
      preferredTemperatureUnit = unitFromSpa;
      localStorage.setItem("layzspaTemperatureUnit", preferredTemperatureUnit);
    }
  }
  render();
}

let livePollTimer = null;
let liveReconnectTimer = null;

function setLiveConnectionStatus(online) {
  const el = document.getElementById("connectionStatus");
  if (!el) return;

  if (online) {
    el.className = "ptw-connection live";
    el.title = tr("Live verbonden");
    el.setAttribute("aria-label", tr("Live verbinding actief"));
  } else {
    el.className = "ptw-connection offline";
    el.title = tr("Offline");
    el.setAttribute("aria-label", tr("Geen live verbinding"));
  }
}

/*
 * REST is alleen nog een fallback wanneer WebSocket niet beschikbaar is.
 * Normaal live verkeer gebruikt /ws en veroorzaakt dus geen 1 s HTTP-poll.
 */
async function pollLiveStateOnce() {
  try {
    const response = await fetch("/api/status", { cache: "no-store" });
    if (!response.ok) throw new Error("status unavailable");
    applyLiveState(await response.json());
  } catch (_) {}
}

function connectWebSocket() {
  clearTimeout(liveReconnectTimer);

  if (socket &&
      (socket.readyState === WebSocket.OPEN ||
       socket.readyState === WebSocket.CONNECTING)) {
    return;
  }

  const scheme = location.protocol === "https:" ? "wss:" : "ws:";
  socket = new WebSocket(`${scheme}//${location.host}/ws`);

  socket.onopen = () => {
    clearTimeout(livePollTimer);
    setLiveConnectionStatus(true);

    // Eerste state pas na voltooide WebSocket-handshake opvragen.
    socket.send("state:get");
  };

  socket.onmessage = (event) => {
    try {
      const message = JSON.parse(event.data);

      if (message && message.wsError) {
        showToast(message.wsError, true);
        return;
      }

      applyLiveState(message);
      setLiveConnectionStatus(true);
    } catch (_) {
      // Niet-JSON WebSocket-berichten worden bewust genegeerd.
    }
  };

  socket.onerror = () => {
    // onclose verzorgt reconnect/fallback.
  };

  socket.onclose = () => {
    setLiveConnectionStatus(false);
    resetLiveControls();

    // Eén lichte REST-fallback als WebSocket wegvalt; geen periodieke polling.
    pollLiveStateOnce();
    liveReconnectTimer = setTimeout(connectWebSocket, 2000);
  };
}

function setText(id, value) {
  const el = document.getElementById(id);
  if (el) el.innerText = value;
}

function setActive(id, active) {
  const el = document.getElementById(id);
  if (!el) return;

  el.classList.toggle("active", active);

  const label = el.querySelector("small");
  if (label) label.innerText = active ? tr("AAN") : tr("UIT");
}

function setDot(id, ok, goodText, badText) {
  const el = document.getElementById(id);
  if (!el) return;

  el.innerText = ok ? goodText : badText;
  el.className = "dot " + (ok ? "green" : "red");
}

function renderGauge() {
  const currentCard = document.getElementById("currentTemperatureCard");
  const targetCard = document.getElementById("targetTemperatureCard");
  const temp = Number(state.temperature);
  const target = Number(state.target);
  const activelyHeating = !!state.heaterActive;
  const targetReached = Number.isFinite(temp) && Number.isFinite(target) && target > 0 && temp >= target;

  [currentCard, targetCard].forEach(card => {
    if (!card) return;
    card.classList.toggle("is-heating", activelyHeating);
    card.classList.toggle("is-ready", !activelyHeating && targetReached);
    card.classList.toggle("is-cold", !activelyHeating && !targetReached);
  });

  const mode = activelyHeating
    ? tr("Verwarmen")
    : (state.heater && targetReached)
      ? tr("Water is op temperatuur")
      : state.heater
        ? tr("Heater stand-by")
        : tr("Stand-by");
  const modeEl = document.getElementById("spaMode");
  if (modeEl) {
    const label = modeEl.querySelector("span:first-child");
    if (label) label.innerText = mode;
    else modeEl.innerText = mode;
    const check = modeEl.querySelector(".status-check");
    if (check) check.style.display = targetReached ? "inline-grid" : "none";
  }

  setText(
    "targetHeatStatus",
    state.heater && targetReached
      ? tr("Water op temperatuur")
      : state.heaterActive
        ? tr("Verwarming actief")
        : state.heater
          ? tr("Verwarming: Stand-by")
          : tr("Verwarming: Uit")
  );
}

function ensureGodModeControl() {
  let button = document.getElementById("controlGodMode");

  /*
   * Geen extra HTML nodig:
   * gebruik dezelfde button/container-stijl als controlHeater.
   */
  if (!button) {
    const heater = document.getElementById("controlHeater");
    if (!heater || !heater.parentElement) return null;

    button = document.createElement("button");
    button.id = "controlGodMode";
    button.type = "button";
    button.className = heater.className;
    button.onclick = () => sendCommand("toggle:godmode");
    button.innerText = tr("Godmode");

    heater.parentElement.insertBefore(
      button,
      heater.nextSibling
    );
  }

  return button;
}

function ensureFullPowerControl() {
  let button = document.getElementById("controlFullPower");

  if (!button) {
    const godMode = ensureGodModeControl();
    if (!godMode || !godMode.parentElement) return null;

    button = document.createElement("button");
    button.id = "controlFullPower";
    button.type = "button";
    button.className = godMode.className;
    button.onclick = () => sendCommand("toggle:fullpower");
    button.innerText = tr("Full Power");

    godMode.parentElement.insertBefore(
      button,
      godMode.nextSibling
    );
  }

  return button;
}


function applyProtocolCapabilities() {
  const is4Wire = state.protocolFamily === "4wire";
  const type2 = state.protocolFamily === "type2";
  const godMode = is4Wire && state.godMode === true;
  const fullPower = godMode && state.fullPower === true;

  /*
   * Godmode bestaat uitsluitend voor 4-wire.
   */
  const godButton = ensureGodModeControl();

  if (godButton) {
    godButton.style.display = is4Wire ? "" : "none";
    godButton.disabled = !is4Wire;

    if (is4Wire) {
      godButton.title = godMode
        ? tr("Godmode is actief")
        : tr("Godmode is uit");

      setControlActive(
        "controlGodMode",
        godMode,
        godMode ? tr("Aan") : tr("Uit")
      );
    }
  }

  /*
   * Full Power:
   * - uitsluitend 4-wire
   * - alleen beschikbaar wanneer Godmode actief is
   */
  const fullPowerButton = ensureFullPowerControl();

  if (fullPowerButton) {
    fullPowerButton.style.display =
      (is4Wire && godMode) ? "" : "none";

    fullPowerButton.disabled =
      !(is4Wire && godMode);

    if (is4Wire && godMode) {
      fullPowerButton.title = fullPower
        ? tr("Full Power is actief")
        : tr("Full Power is uit");

      setControlActive(
        "controlFullPower",
        fullPower,
        fullPower ? tr("Aan") : tr("Uit")
      );
    } else {
      setControlActive(
        "controlFullPower",
        false,
        tr("Uit")
      );
    }
  }

  /*
   * Power:
   * - 4-wire: geen zelfstandige bewezen Power-bediening
   * - Type2: status niet betrouwbaar schakelbaar
   * - Type1: ondersteund
   */
  ["power", "controlPower"].forEach(id => {
    const el = document.getElementById(id);
    if (!el) return;

    const disabled = is4Wire || type2;
    el.disabled = disabled;

    if (is4Wire) {
      el.title = tr(
        "Power heeft voor 4-wire geen zelfstandige bediening"
      );
    } else if (type2) {
      el.title = tr(
        "Power is voor Type2 niet bestuurbaar"
      );
    } else {
      el.removeAttribute("title");
    }
  });

  /*
   * Heater:
   * - 4-wire: beschikbaar in Godmode
   * - Type1/Type2: bestaande bediening behouden
   */
  ["heater", "controlHeater"].forEach(id => {
    const el = document.getElementById(id);
    if (!el) return;

    const disabled = is4Wire && !godMode;
    el.disabled = disabled;

    if (disabled) {
      el.title = tr(
        "Schakel Godmode in om de 4-wire verwarming te bedienen"
      );
    } else {
      el.removeAttribute("title");
    }
  });

  /*
   * Target +/-:
   * - 4-wire: beschikbaar in Godmode
   * - Type1/Type2: bestaande fysieke pompbediening
   */
  document
    .querySelectorAll(
      "#targetTemperatureCard .temperature-card-controls button"
    )
    .forEach(button => {
      const disabled = is4Wire && !godMode;
      button.disabled = disabled;

      if (disabled) {
        button.title = tr(
          "Schakel Godmode in om de 4-wire doeltemperatuur te wijzigen"
        );
      } else {
        button.removeAttribute("title");
      }
    });
}

function resetLiveControls() {
  ["heater", "filter", "bubbles", "jets", "power"].forEach(id => setActive(id, false));
  setControlActive("controlPower", false, tr("Uit"));
  setControlActive("controlLock", false, tr("Vrij"));
  setControlActive("controlUnit", false, "°C");
  setControlActive("controlTimer", false, tr("Uit"));
  setControlActive("controlHeater", false, tr("Uit"));
  setControlActive("controlGodMode", false, tr("Uit"));
  setControlActive("controlFullPower", false, tr("Uit"));
  setControlActive("controlFilter", false, tr("Uit"));
  setControlActive("controlBubbles", false, tr("Uit"));
  setControlActive("controlJets", false, tr("Uit"));
  document.getElementById("controlHeater")?.classList.remove("heating-now");
}

function render() {
  applyProtocolCapabilities();

  setText("temperature", displayTemperature(state.temperature ?? "--"));
  setText("tempSmall", displayTemperature(state.temperature ?? "--"));
  setText("target", displayTemperature(state.target ?? "--"));
  setText("targetBig", displayTemperature(state.target ?? "--"));
  applyTemperatureUnit();
  setText("ip", state.ip ?? "--");
  setText("uptime", state.uptime ?? "--");

  if (!state.dataValid) {
    resetLiveControls();
    renderGauge();
    return;
  }

  setActive("heater", state.heater);
  const heaterButton = document.getElementById("heater");
  if (heaterButton) {
    heaterButton.classList.toggle("heating-now", !!state.heaterActive);
    const label = heaterButton.querySelector("small");
    if (label) label.innerText = state.heaterActive ? tr("VERWARMT") : (state.heater ? tr("AAN") : tr("UIT"));
  }
  setActive("filter", state.filter);
  setActive("bubbles", state.bubbles);
  setActive("jets", state.jets);
  setActive("power", state.power);
  setControlActive("controlPower", state.power, state.power ? tr("Aan") : tr("Uit"));
  setControlActive("controlLock", state.locked, state.locked ? tr("Vergrendeld") : tr("Vrij"));
  setControlActive("controlUnit", state.fahrenheit, state.fahrenheit ? "°F" : "°C");
  setControlActive("controlTimer", state.timerActive, state.timerActive ? tr("Actief") : tr("Uit"));
  setControlActive("controlHeater", state.heater, state.heaterActive ? tr("Verwarmt") : (state.heater ? tr("Stand-by") : tr("Uit")));
  setControlActive(
    "controlFullPower",
    !!state.fullPower,
    state.fullPower ? tr("Aan") : tr("Uit")
  );
  document.getElementById("controlHeater")?.classList.toggle("heating-now", !!state.heaterActive);
  setControlActive("controlFilter", state.filter, state.filter ? tr("Aan") : tr("Uit"));
  setControlActive("controlBubbles", state.bubbles, state.bubbles ? tr("Aan") : tr("Uit"));
  setControlActive("controlJets", state.jets, state.jets ? tr("Aan") : tr("Uit"));

  renderSpaError(state.errorCode);

  setDot("wifiStatus", !!state.wifi, tr("Verbonden"), tr("Offline"));
  setDot("mqttStatus", !!state.mqtt, tr("Online"), tr("Offline"));

  setDot(
    "settingsWifiStatus",
    !!state.wifi,
    "Verbonden",
    "Offline"
  );

  setText("settingsIp", state.ip ?? "--");
  setText("settingsUptime", state.uptime ?? "--");
  setText(
    "settingsHeap",
    state.heap ? formatBytes(state.heap) : "--"
  );

  setText("infoWifi", state.wifi ? tr("Verbonden") : tr("Offline"));
  setText("infoRssi", state.wifi && Number.isFinite(Number(state.rssi)) ? `${state.rssi} dBm` : "--");
  setText("infoMqtt", state.mqtt ? tr("Verbonden") : tr("Offline"));
  setText("infoIp", state.ip ?? "--");
  setText("infoUptime", state.uptime ?? "--");
  setText("infoHeap", state.heap ? formatBytes(state.heap) : "--");
  setText("infoChip", state.chipId ? String(state.chipId).toUpperCase() : "--");

  renderGauge();
}

function renderSpaError(errorCode) {
  const code = Number(errorCode || 0);

  let alert = document.getElementById("spaErrorAlert");

  if (!alert) {
    const statusStrip =
      document.querySelector(".dashboard-status-strip");

    const dashboard =
      document.getElementById("dashboardView");

    if (!dashboard) return;

    alert = document.createElement("div");
    alert.id = "spaErrorAlert";

    alert.style.margin = "12px 0";
    alert.style.padding = "12px 16px";
    alert.style.borderRadius = "12px";
    alert.style.fontWeight = "700";
    alert.style.textAlign = "center";
    alert.style.background = "rgba(220, 53, 69, 0.15)";
    alert.style.border = "1px solid rgba(220, 53, 69, 0.45)";

    if (statusStrip) {
      statusStrip.insertAdjacentElement("afterend", alert);
    } else {
      dashboard.prepend(alert);
    }
  }

  if (code > 0) {
    alert.textContent =
      `⚠ ${tr("Spa fout")} E${String(code).padStart(2, "0")}`;

    alert.hidden = false;
  } else {
    alert.hidden = true;
  }
}

function setControlActive(id, active, text) {
  const el = document.getElementById(id);
  if (!el) return;
  el.classList.toggle("active", !!active);
  const small = el.querySelector("small");
  if (small) small.innerText = text;
}

function pressSpaButton(button) {
  sendCommand("press:" + button);
}


const navDefaults = {dashboard:true,control:true,planner:true,history:true,energy:true,logs:true,maintenance:true,settings:true,info:true};
const dashboardSectionDefaults = {status:true,currentTemperature:true,targetTemperature:true,weather:true,controls:true,summary:true,insight:true,system:true};
const dashboardOrderDefaults = ["status","currentTemperature","targetTemperature","weather","controls","summary","insight","system"];
function getDashboardOrder(){
  try{
    const stored=JSON.parse(localStorage.getItem("layzspaDashboardOrder")||"[]");
    const valid=stored.filter(k=>dashboardOrderDefaults.includes(k));
    dashboardOrderDefaults.forEach(k=>{if(!valid.includes(k))valid.push(k);});
    return valid;
  }catch(_){return [...dashboardOrderDefaults];}
}
function applyDashboardOrder(){
  const dashboard=document.getElementById("dashboardView");
  if(!dashboard)return;
  getDashboardOrder().forEach(key=>{
    const card=dashboard.querySelector(`[data-dashboard-card="${key}"]`);
    if(card)dashboard.appendChild(card);
  });
}
function loadDashboardOrderEditor(){
  const list=document.getElementById("dashboardOrderList");
  if(!list)return;
  getDashboardOrder().forEach(key=>{
    const item=list.querySelector(`[data-dashboard-order="${key}"]`);
    if(item)list.appendChild(item);
  });
  initializeDashboardOrderDrag();
}
function initializePointerSortable(list, itemSelector, handleSelector){
  if(!list||list.dataset.pointerSortReady==="true")return;
  list.dataset.pointerSortReady="true";
  let dragged=null;
  let pointerId=null;
  let startY=0;
  let active=false;

  list.querySelectorAll(itemSelector).forEach(item=>item.setAttribute("draggable","false"));

  list.addEventListener("pointerdown",event=>{
    const handle=event.target.closest(handleSelector);
    const item=event.target.closest(itemSelector);
    if(!handle||!item||!list.contains(item))return;
    dragged=item;
    pointerId=event.pointerId;
    startY=event.clientY;
    active=false;
    handle.setPointerCapture?.(pointerId);
    event.preventDefault();
  });

  list.addEventListener("pointermove",event=>{
    if(!dragged||event.pointerId!==pointerId)return;
    if(!active&&Math.abs(event.clientY-startY)<4)return;
    active=true;
    dragged.classList.add("dragging");
    const siblings=[...list.querySelectorAll(`${itemSelector}:not(.dragging)` )];
    let placed=false;
    for(const sibling of siblings){
      const box=sibling.getBoundingClientRect();
      if(event.clientY<box.top+box.height/2){
        list.insertBefore(dragged,sibling);
        placed=true;
        break;
      }
    }
    if(!placed)list.appendChild(dragged);
    event.preventDefault();
  });

  const finish=event=>{
    if(!dragged||event.pointerId!==pointerId)return;
    dragged.classList.remove("dragging");
    dragged=null;
    pointerId=null;
    active=false;
    event.preventDefault();
  };
  list.addEventListener("pointerup",finish);
  list.addEventListener("pointercancel",finish);
}

function initializeDashboardOrderDrag(){
  initializePointerSortable(document.getElementById("dashboardOrderList"), ".dashboard-order-item", ".drag-handle");
}
function saveDashboardOrder(){
  const list=document.getElementById("dashboardOrderList");
  if(!list)return;
  const order=[...list.querySelectorAll("[data-dashboard-order]")].map(el=>el.dataset.dashboardOrder);
  localStorage.setItem("layzspaDashboardOrder",JSON.stringify(order));
  applyDashboardOrder();
  showToast(tr("Dashboardvolgorde opgeslagen"));
}
function resetDashboardOrder(){
  localStorage.removeItem("layzspaDashboardOrder");
  loadDashboardOrderEditor();
  applyDashboardOrder();
  showToast(tr("Standaardvolgorde hersteld"));
}

const dashboardButtonOrderDefaults = ["heater","filter","bubbles","jets","power"];
function getDashboardButtonOrder(){
  try{
    const stored=JSON.parse(localStorage.getItem("layzspaDashboardButtonOrder")||"[]");
    const valid=stored.filter(k=>dashboardButtonOrderDefaults.includes(k));
    dashboardButtonOrderDefaults.forEach(k=>{if(!valid.includes(k))valid.push(k);});
    return valid;
  }catch(_){return [...dashboardButtonOrderDefaults];}
}
function applyDashboardButtonOrder(){
  const grid=document.querySelector(".premium-device-grid");
  if(!grid)return;
  getDashboardButtonOrder().forEach(key=>{
    const button=grid.querySelector(`[data-dashboard-button-order="${key}"]`);
    if(button)grid.appendChild(button);
  });
}
function loadDashboardButtonOrderEditor(){
  const list=document.getElementById("dashboardButtonOrderList");
  if(!list)return;
  getDashboardButtonOrder().forEach(key=>{
    const item=list.querySelector(`[data-dashboard-button-order-item="${key}"]`);
    if(item)list.appendChild(item);
  });
  initializeDashboardButtonOrderDrag();
}
function initializeDashboardButtonOrderDrag(){
  initializePointerSortable(document.getElementById("dashboardButtonOrderList"), ".dashboard-order-item", ".drag-handle");
}
function saveDashboardButtonOrder(){
  const list=document.getElementById("dashboardButtonOrderList");
  if(!list)return;
  const order=[...list.querySelectorAll("[data-dashboard-button-order-item]")].map(el=>el.dataset.dashboardButtonOrderItem);
  localStorage.setItem("layzspaDashboardButtonOrder",JSON.stringify(order));
  applyDashboardButtonOrder();
  showToast(tr("Knoppenvolgorde opgeslagen"));
}
function resetDashboardButtonOrder(){
  localStorage.removeItem("layzspaDashboardButtonOrder");
  loadDashboardButtonOrderEditor();
  applyDashboardButtonOrder();
  showToast(tr("Standaardvolgorde hersteld"));
}

function readLocalConfig(key, defaults){ try{return {...defaults,...JSON.parse(localStorage.getItem(key)||"{}")};}catch(_){return {...defaults};} }
const navigationOrderDefaults=["dashboard","control","planner","history","energy","logs","maintenance","settings","info"];
function getNavigationOrder(){
  try{
    const stored=JSON.parse(localStorage.getItem("layzspaNavigationOrder")||"[]");
    const valid=stored.filter(name=>navigationOrderDefaults.includes(name));
    navigationOrderDefaults.forEach(name=>{if(!valid.includes(name))valid.push(name);});
    return valid;
  }catch(_){return [...navigationOrderDefaults];}
}
function applyNavigationOrder(){
  const nav=document.getElementById("bottomNav");
  if(nav)getNavigationOrder().forEach(name=>{const button=document.getElementById(`nav${name.charAt(0).toUpperCase()+name.slice(1)}`);if(button)nav.appendChild(button);});
}
function loadNavigationOrderEditor(){
  const list=document.getElementById("navigationOrderList");
  if(!list)return;
  getNavigationOrder().forEach(name=>{const item=list.querySelector(`[data-nav-order="${name}"]`);if(item)list.appendChild(item);});
  initializeNavigationOrderDrag();
}
function initializeNavigationOrderDrag(){
  initializePointerSortable(document.getElementById("navigationOrderList"), ".navigation-order-item", ".drag-handle");
}
function resetNavigationOrder(){
  localStorage.removeItem("layzspaNavigationOrder");
  loadNavigationOrderEditor();
  applyNavigationOrder();
  showToast(tr("Standaardvolgorde hersteld"));
}
function applyNavigationSettings(){
  const cfg=readLocalConfig("layzspaNavTabs",navDefaults);
  const enabled=localStorage.getItem("layzspaBottomNavEnabled")!=="false";
  applyNavigationOrder();
  document.getElementById("bottomNav")?.classList.toggle("bottom-nav-disabled",!enabled);
  document.body.classList.toggle("without-bottom-nav",!enabled);
  const toggle=document.getElementById("bottomNavEnabled"); if(toggle) toggle.checked=enabled;
  Object.keys(navDefaults).forEach(name=>{const el=document.getElementById(`nav${name.charAt(0).toUpperCase()+name.slice(1)}`);if(el)el.classList.toggle("nav-user-hidden",cfg[name]===false);});
  document.querySelectorAll("[data-nav-setting]").forEach(i=>i.checked=cfg[i.dataset.navSetting]!==false);
}
function saveNavigationSettings(){
  const cfg={...navDefaults};
  document.querySelectorAll("[data-nav-setting]").forEach(i=>cfg[i.dataset.navSetting]=i.checked);
  const list=document.getElementById("navigationOrderList");
  const order=list?[...list.querySelectorAll("[data-nav-order]")].map(el=>el.dataset.navOrder):getNavigationOrder();
  localStorage.setItem("layzspaNavTabs",JSON.stringify(cfg));
  localStorage.setItem("layzspaNavigationOrder",JSON.stringify(order));
  localStorage.setItem("layzspaBottomNavEnabled",document.getElementById("bottomNavEnabled")?.checked!==false?"true":"false");
  applyNavigationSettings(); showToast(tr("Navigatie opgeslagen"));
}
function getDashboardSections(){
  const cfg=readLocalConfig("layzspaDashboardSections",dashboardSectionDefaults);
  // Oude instelling uit eerdere versies blijven respecteren.
  if(cfg.gauge===false){cfg.currentTemperature=false;cfg.targetTemperature=false;}
  return cfg;
}
function applyDashboardSections(){
  const cfg=getDashboardSections();
  const map={
    status:".dashboard-status-strip",
    currentTemperature:"#currentTemperatureCard",
    targetTemperature:"#targetTemperatureCard",
    weather:"#weatherDashboardCard",
    controls:".premium-device-grid",
    summary:"#smartSummary",
    insight:"#smartInsightCard",
    system:".premium-system-list"
  };
  Object.entries(map).forEach(([k,sel])=>document.querySelector(sel)?.classList.toggle("dashboard-section-hidden",cfg[k]===false));
  document.querySelectorAll("[data-dashboard-section-setting]").forEach(i=>i.checked=cfg[i.dataset.dashboardSectionSetting]!==false);
}
function saveDashboardLayout(){
  saveDashboardButtons(false);
  const cfg={...dashboardSectionDefaults};
  document.querySelectorAll("[data-dashboard-section-setting]").forEach(i=>cfg[i.dataset.dashboardSectionSetting]=i.checked);
  localStorage.setItem("layzspaDashboardSections",JSON.stringify(cfg));
  applyDashboardSections();
  showToast(tr("Dashboardindeling opgeslagen"));
}
function setAllDashboardVisibility(enabled){
  document.querySelectorAll("[data-dashboard-setting],[data-dashboard-section-setting]").forEach(i=>i.checked=enabled);
}
function resetDashboardVisibility(){
  document.querySelectorAll("[data-dashboard-setting],[data-dashboard-section-setting]").forEach(i=>i.checked=true);
  localStorage.removeItem("layzspaDashboardButtons");
  localStorage.removeItem("layzspaDashboardSections");
  applyDashboardButtons();
  applyDashboardSections();
  showToast(tr("Standaardindeling hersteld"));
}
function toggleAppMenu(){const open=!document.getElementById("appMenu")?.classList.contains("open");document.getElementById("appMenu")?.classList.toggle("open",open);document.getElementById("appMenuBackdrop")?.classList.toggle("open",open);document.getElementById("appMenu")?.setAttribute("aria-hidden",open?"false":"true");}
function closeAppMenu(){document.getElementById("appMenu")?.classList.remove("open");document.getElementById("appMenuBackdrop")?.classList.remove("open");document.getElementById("appMenu")?.setAttribute("aria-hidden","true");}
function openMenuView(name){closeAppMenu();showView(name);}

const dashboardButtonDefaults = { power: true, heater: true, filter: true, bubbles: true, jets: true };

function getDashboardButtons() {
  try {
    return { ...dashboardButtonDefaults, ...JSON.parse(localStorage.getItem("layzspaDashboardButtons") || "{}") };
  } catch (_) {
    return { ...dashboardButtonDefaults };
  }
}

function applyDashboardButtons() {
  const cfg = getDashboardButtons();
  document.querySelectorAll("[data-dashboard-button]").forEach(el => {
    el.classList.toggle("dashboard-button-hidden", cfg[el.dataset.dashboardButton] === false);
  });
}

function loadDashboardButtons() {
  const cfg = getDashboardButtons();
  document.querySelectorAll("[data-dashboard-setting]").forEach(input => {
    input.checked = cfg[input.dataset.dashboardSetting] !== false;
  });
  applyDashboardButtons();
}

function saveDashboardButtons(showMessage = true) {
  const cfg = {};
  document.querySelectorAll("[data-dashboard-setting]").forEach(input => {
    cfg[input.dataset.dashboardSetting] = input.checked;
  });
  localStorage.setItem("layzspaDashboardButtons", JSON.stringify(cfg));
  applyDashboardButtons();
  if (showMessage) showToast(tr("Dashboardknoppen opgeslagen"));
}

function sendCommand(command) {
  // Laat de knop meteen omschakelen; WebSocket/pompbevestiging corrigeert
  // de status automatisch als de fysieke pomp anders reageert.
  applyOptimisticCommand(command);

  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.send(command);
    return;
  }

  // Veilige fallback als de browser geen actieve WebSocket heeft.
  fetch("/api/command", {
    method: "POST",
    headers: {"Content-Type": "text/plain;charset=UTF-8"},
    body: command
  }).then(async response => {
    if (!response.ok) throw new Error(await response.text());
    return response.json();
  }).then(applyLiveState).catch(error => showToast(error.message || tr("Opdracht mislukt"), true));
}

function toggleDevice(device) {
  sendCommand("toggle:" + device);
}

function setTarget(change) {
  sendCommand(change > 0 ? "target:+" : "target:-");
}

// Alleen bij openen ophalen; geen periodieke HTTP-polling.
// Eén gedeelde taak voorkomt overlap bij herhaald tikken op Dashboard.
let dashboardDetailsRequest = null;
function loadDashboardDetails() {
  if (dashboardDetailsRequest) return dashboardDetailsRequest;
  dashboardDetailsRequest = (async () => {
    // Achter elkaar om tijdelijke HTTP/JSON-heapdruk te beperken.
    await loadSystemStatus();
    await loadEnergy();
  })().finally(() => { dashboardDetailsRequest = null; });
  return dashboardDetailsRequest;
}

function showView(viewName) {
  const titles = {
    dashboard: "Bestway Lay-Z-Spa",
    planner: "Planner",
    history: "Historie",
    energy: "Energie",
    logs: "Diagnostiek",
    settings: "Instellingen",
    hardware: "Hardware",
    hardwareTest: "Connectortest",
    control: "Bedieningspaneel",
    maintenance: "Onderhoud",
    personalization: "Interface aanpassen",
    info: "Informatie"
  };

  document.querySelectorAll(".view").forEach((view) => {
    view.classList.toggle("active", view.id === `${viewName}View`);
  });

  document.querySelectorAll(".bottom-nav button").forEach((button) => {
    button.classList.toggle("selected", button.id === `nav${viewName.charAt(0).toUpperCase()}${viewName.slice(1)}`);
  });

  setText("pageTitle", titles[viewName] || "Bestway Lay-Z-Spa");

  if (viewName === "dashboard") {
    loadDashboardDetails();
  } else if (viewName === "settings") {
    loadWifiStatus();
    loadMatterSettings();
    loadMqttSettings();
    loadRegionalSettings();
    loadWeatherSettings();
    loadSystemStatus();
  } else if (viewName === "info") {
    // De Info-pagina gebruikt systeemvelden die door /api/wifi/status worden gevuld.
    // Haal ze bij het openen altijd opnieuw op, ook als Instellingen nog niet is bezocht.
    loadWifiStatus();
    render();
  } else if (viewName === "hardware") {
    loadHardwareSettings();
  } else if (viewName === "maintenance") {
    loadMaintenance();
  } else if (viewName === "control") {
    // Bedieningspaneel blijft een zuiver bedieningsscherm.
  } else if (viewName === "personalization") {
    loadDashboardButtons();
    applyDashboardButtonOrder();
    loadDashboardButtonOrderEditor();
    applyDashboardSections();
    applyNavigationSettings();
    loadNavigationOrderEditor();
  } else if (viewName === "planner") {
    loadSchedules();
  } else if (viewName === "history") {
    loadHistory();
  } else if (viewName === "energy") {
    loadEnergy();
  } else if (viewName === "logs") {
    loadDiagnostics();
    loadLoggingState().then(() => {
      if (debugLoggingEnabled) loadLogs();
      else {
        logData = [];
        renderLogs();
      }
    });
  }

  window.scrollTo({ top: 0, behavior: "smooth" });
}


async function loadWifiStatus() {
  try {
    const response = await fetch("/api/wifi/status", {
      cache: "no-store"
    });

    if (!response.ok) {
      throw new Error("WiFi-status niet beschikbaar");
    }

    const wifi = await response.json();

    setDot(
      "settingsWifiStatus",
      !!wifi.connected,
      tr("Verbonden"),
      tr("Offline")
    );

    setText("settingsSsid", wifi.ssid || "--");
    setText("settingsIp", wifi.ip || "--");
    setText(
      "settingsRssi",
      wifi.connected ? `${wifi.rssi} dBm` : "--"
    );

    setText("settingsHostname", wifi.hostname || "--");
    setText("settingsMac", wifi.mac || "--");
    setText("settingsApActive", wifi.apMode ? tr("Ja") : tr("Nee"));
    setText("settingsApSsid", wifi.apSsid || "--");
    setText("settingsApIp", wifi.apIp || "--");

    setText("settingsFirmware", wifi.firmware || "--");
    setText("infoFirmware", wifi.firmware || "--");
    setText("settingsChip", wifi.chip || "ESP32-C6");
    setText("settingsOtaPartition", wifi.otaPartition || "--");
    setText("settingsResetReason", wifi.resetReason || "--");

    const wifiParams = new URLSearchParams(window.location.search);

    if (wifiParams.get("wifi") === "success") {
      showToast(
        `${tr("WiFi-instellingen toegepast. IP-adres:")} ${wifi.ip || "--"}`
      );

      wifiParams.delete("wifi");

      const cleanQuery = wifiParams.toString();
      const cleanUrl =
        window.location.pathname +
        (cleanQuery ? `?${cleanQuery}` : "") +
        window.location.hash;

      window.history.replaceState({}, "", cleanUrl);
    }
  } catch (error) {
    showToast(error.message, true);
  }
}

async function openWifiModal() {
  const modal = document.getElementById("wifiModal");

  modal.classList.add("open");
  modal.setAttribute("aria-hidden", "false");
  document.body.style.overflow = "hidden";

  await scanWifiNetworks();
}

function closeWifiModal() {
  const modal = document.getElementById("wifiModal");

  modal.classList.remove("open");
  modal.setAttribute("aria-hidden", "true");
  document.body.style.overflow = "";

  resetWifiModal();
}

function resetWifiModal() {
  selectedNetwork = "";

  document
    .getElementById("wifiForm")
    .classList.add("hidden");

  document
    .getElementById("wifiNetworks")
    .classList.remove("hidden");

  document.getElementById("selectedSsid").value = "";
  document.getElementById("wifiPassword").value = "";
  document.getElementById("wifiIpMode").value = "dhcp";
  ["wifiStaticIp","wifiGateway","wifiDns1","wifiDns2"].forEach(id => document.getElementById(id).value = "");
  document.getElementById("wifiSubnet").value = "255.255.255.0";
  document.getElementById("wifiConnectProgress")?.classList.add("hidden");
  toggleWifiStaticFields();
}
async function scanWifiNetworks() {
  const scanState = document.getElementById("wifiScanState");
  const list = document.getElementById("wifiNetworks");
  const form = document.getElementById("wifiForm");

  form.classList.add("hidden");
  list.classList.remove("hidden");
  list.innerHTML = "";

  scanState.classList.remove("hidden");
  scanState.innerText = tr("Netwerken zoeken...");

  const startedAt = Date.now();
  const timeoutMs = 20000;

  try {
    while (Date.now() - startedAt < timeoutMs) {
      const response = await fetch("/api/wifi/scan", {
        cache: "no-store"
      });

      const result = await response.json();

      if (
        response.status === 202 &&
        result.status === "scanning"
      ) {
        await sleep(400);
        continue;
      }

      if (!response.ok) {
        throw new Error(
          result.error || tr("Scannen mislukt")
        );
      }

      const networks = result;

      scanState.classList.add("hidden");

      if (
        !Array.isArray(networks) ||
        networks.length === 0
      ) {
        scanState.classList.remove("hidden");
        scanState.innerText = tr("Geen netwerken gevonden.");
        return;
      }

      networks
        .sort(
          (a, b) =>
            Number(b.rssi) - Number(a.rssi)
        )
        .forEach((network) => {
          const button =
            document.createElement("button");

          button.type = "button";
          button.className = "network-item";

          const security = network.encrypted
            ? tr("Beveiligd")
            : tr("Open netwerk");

          button.innerHTML = `
            <span class="network-name">
              <b>${escapeHtml(
                network.ssid || tr("Verborgen netwerk")
              )}</b>
              <small>${security}</small>
            </span>
            <span class="signal">
              ${signalBars(Number(network.rssi))}
              ${Number(network.rssi)} dBm
            </span>
          `;

          button.addEventListener("click", () => {
            selectWifiNetwork(network);
          });

          list.appendChild(button);
        });

      return;
    }

    throw new Error(tr("WiFi-scan duurde te lang"));
  } catch (error) {
    scanState.classList.remove("hidden");
    scanState.innerText = error.message;

    showToast(error.message, true);
  }
}

function selectWifiNetwork(network) {
  selectedNetwork = network.ssid || "";

  const currentSsid =
    document.getElementById("settingsSsid")?.textContent?.trim() || "";

  const sameNetwork =
    currentSsid &&
    currentSsid !== "--" &&
    selectedNetwork === currentSsid;

  document.getElementById("selectedSsid").value =
    selectedNetwork;

  document
    .getElementById("wifiNetworks")
    .classList.add("hidden");

  document
    .getElementById("wifiScanState")
    .classList.add("hidden");

  document
    .getElementById("wifiForm")
    .classList.remove("hidden");

  const password =
    document.getElementById("wifiPassword");

  password.required = !!network.encrypted;

  password.required =
    !!network.encrypted && !sameNetwork;

  password.placeholder =
    sameNetwork
      ? tr("Niet nodig bij hetzelfde netwerk")
      : network.encrypted
        ? tr("WiFi-wachtwoord")
        : tr("Niet nodig voor open netwerk");

  if (sameNetwork) {
    password.value = "";
  }

  password.focus();
}

function backToNetworks() {
  document
    .getElementById("wifiForm")
    .classList.add("hidden");

  document
    .getElementById("wifiNetworks")
    .classList.remove("hidden");
}


function toggleWifiStaticFields() {
  const isStatic = document.getElementById("wifiIpMode")?.value === "static";
  const fields = document.getElementById("wifiStaticFields");
  if (!fields) return;
  fields.classList.toggle("hidden", !isStatic);
  ["wifiStaticIp", "wifiGateway", "wifiSubnet"].forEach(id => {
    const input = document.getElementById(id);
    if (input) input.required = isStatic;
  });
}

function isValidIpv4(value) {
  const parts = String(value || "").trim().split(".");
  return parts.length === 4 && parts.every(part => /^\d{1,3}$/.test(part) && Number(part) >= 0 && Number(part) <= 255);
}

async function connectSelectedWifi(event) {
  event.preventDefault();
  const password=document.getElementById("wifiPassword").value;
  const button=document.getElementById("wifiConnectButton");
  const backButton=document.getElementById("wifiBackButton");
  if(!selectedNetwork){showToast(tr("Kies eerst een netwerk."),true);return;}
  const mode=document.getElementById("wifiIpMode").value;
  const ip=document.getElementById("wifiStaticIp").value.trim();
  const gateway=document.getElementById("wifiGateway").value.trim();
  const subnet=document.getElementById("wifiSubnet").value.trim();
  const dns1=document.getElementById("wifiDns1").value.trim();
  const dns2=document.getElementById("wifiDns2").value.trim();
  if(mode==="static"&&(![ip,gateway,subnet].every(isValidIpv4)||(dns1&&!isValidIpv4(dns1))||(dns2&&!isValidIpv4(dns2)))){showToast(tr("Controleer de vaste IP-instellingen."),true);return;}
  button.disabled=true;if(backButton)backButton.disabled=true;button.innerText=tr("Verbinden...");
  setWifiConnectProgress("connecting",tr("Verbinden met WiFi…"),tr("De Spa Control probeert verbinding te maken met het gekozen netwerk."));
  const body=new URLSearchParams({ssid:selectedNetwork,password,mode,ip,gateway,subnet,dns1,dns2});
  try{
    const response=await fetch("/api/wifi/connect",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:body.toString()});
    const result=await response.json();
    if(!response.ok||!result.ok)throw new Error(result.error||tr("Verbinden kon niet starten"));
    if(result.state==="reconfiguring"){
      const target=(mode==="static"&&ip)?ip:"bestwayspa.local";

      setWifiConnectProgress(
        "success",
        tr("WiFi-instellingen opgeslagen"),
        mode==="static"
          ? tr("Het vaste IP-adres wordt nu toegepast.")
          : tr("DHCP wordt nu toegepast."),
        mode==="static"?ip:""
      );

      showToast(
        mode==="static"
          ? `${tr("Gelukt. Nieuw IP-adres:")} ${ip}`
          : tr("Gelukt. DHCP is ingeschakeld.")
      );

      const expectedIp = mode==="static" ? ip : "";

      const waitForNewAddress = async () => {
        const started = Date.now();

        while (Date.now() - started < 10000) {
          await sleep(500);

          try {
            const response = await fetch(
              "/api/wifi/status",
              { cache: "no-store" }
            );

            if (response.ok) {
              const status = await response.json();

              if (
                !expectedIp ||
                status.ip === expectedIp
              ) {
                window.location.href =
                  "http://bestwayspa.local/";
                return;
              }
            }
          } catch (_) {
            // Tijdens de IP-wissel is een korte netwerkfout normaal.
          }
        }

        // Fallback: na maximaal 10 seconden alsnog naar mDNS.
        window.location.href =
          "http://bestwayspa.local/";
      };

      waitForNewAddress();

      return;
    }

    setWifiConnectProgress("connecting",tr("Wachten op verbinding…"),tr("Dit kan enkele seconden duren. Laat dit venster open."));
    const finalStatus=await waitForWifiConnection(22000);
    if(!finalStatus.connected){
      const message=finalStatus.error||tr("Verbinden mislukt. Controleer het wachtwoord.");
      setWifiConnectProgress("error",tr("Verbinden mislukt"),message+" "+tr("LayZSpa-Setup blijft beschikbaar om het opnieuw te proberen."));
      showToast(message,true);return;
    }
    const newIp=finalStatus.ip||"";
    setWifiConnectProgress(
      "success",
      tr("Verbonden met WiFi"),
      tr("De verbinding is gelukt."),
      newIp
    );

    showToast(
      `${tr("Verbonden. Nieuw IP:")} ${newIp||tr("onbekend")}`
    );
  }catch(error){
    // Tijdens een IP-wissel kan de huidige HTTP-verbinding bewust
    // verdwijnen voordat de browser het laatste antwoord heeft verwerkt.
    // Bij een geldig vast IP behandelen we dat daarom als verwachte
    // netwerkonderbreking en gaan we automatisch naar het nieuwe adres.
    if(mode==="static" && isValidIpv4(ip)){
      setWifiConnectProgress(
        "connecting",
        tr("Netwerkadres wordt gewijzigd…"),
        `${tr("Spa Control schakelt over naar")} ${ip}. ${tr("Een korte onderbreking is normaal.")}`,
        ip
      );

      showToast(
        `${tr("Overschakelen naar nieuw IP-adres:")} ${ip}`
      );

      const expectedIp = mode==="static" ? ip : "";

      const waitForNewAddress = async () => {
        const started = Date.now();

        while (Date.now() - started < 10000) {
          await sleep(500);

          try {
            const response = await fetch(
              "/api/wifi/status",
              { cache: "no-store" }
            );

            if (response.ok) {
              const status = await response.json();

              if (
                !expectedIp ||
                status.ip === expectedIp
              ) {
                window.location.href =
                  "http://bestwayspa.local/";
                return;
              }
            }
          } catch (_) {
            // Tijdens de IP-wissel is een korte netwerkfout normaal.
          }
        }

        // Fallback: na maximaal 10 seconden alsnog naar mDNS.
        window.location.href =
          "http://bestwayspa.local/";
      };

      waitForNewAddress();

      return;
    }

    const message=(error&&error.message)?error.message:tr("Verbinding kon niet worden gecontroleerd.");
    setWifiConnectProgress(
      "error",
      tr("Verbinding kon niet worden gecontroleerd"),
      message+" "+tr("Controleer of LazySpaSetup nog beschikbaar is en probeer zo nodig opnieuw.")
    );
    showToast(message,true);
  }finally{
    button.disabled=false;
    if(backButton)backButton.disabled=false;
    button.innerText=tr("Verbinden");
  }
}

function setWifiConnectProgress(state,title,detail,ip=""){
  const panel=document.getElementById("wifiConnectProgress");if(!panel)return;
  panel.classList.remove("hidden","connecting","success","error");panel.classList.add(state||"connecting");
  setText("wifiConnectProgressTitle",title||"");setText("wifiConnectProgressDetail",detail||"");
  const ipWrap=document.getElementById("wifiConnectIpWrap"),ipValue=document.getElementById("wifiConnectIp"),openButton=document.getElementById("wifiOpenSpaButton");
  const hasIp=!!ip&&ip!=="0.0.0.0";if(ipWrap)ipWrap.classList.toggle("hidden",!hasIp);if(ipValue)ipValue.textContent=hasIp?ip:"--";
  if(openButton){openButton.classList.toggle("hidden",!hasIp);openButton.href=hasIp?`http://${ip}/`:"#";openButton.textContent=tr("Open Spa Control");}
}


async function forgetWifi() {
  const confirmed = confirm(
    tr("Weet je zeker dat je de opgeslagen WiFi-instellingen wilt wissen?") + "\n\n" +
      tr("De ESP schakelt daarna terug naar de setupmodus.")
  );

  if (!confirmed) {
    return;
  }

  try {
    const response = await fetch(
      "/api/wifi/forget",
      {
        method: "POST"
      }
    );

    const result = await response.json();

    if (!response.ok || !result.ok) {
      throw new Error(
        result.error ||
          "WiFi-instellingen konden niet worden gewist."
      );
    }

    showToast(tr("WiFi-instellingen gewist."));

    setDot(
      "settingsWifiStatus",
      false,
      tr("Verbonden"),
      tr("Offline")
    );

    setText("settingsSsid", "--");
    setText("settingsIp", "--");
    setText("settingsRssi", "--");

    setTimeout(() => {
      loadWifiStatus();
    }, 1000);
  } catch (error) {
    showToast(error.message, true);
  }
}
async function waitForWifiConnection(timeoutMs) {
  const startedAt=Date.now();let lastStatus=null;
  while(Date.now()-startedAt<timeoutMs){
    await sleep(500);
    try{
      const response=await fetch("/api/wifi/status",{cache:"no-store"});
      if(!response.ok)continue;
      const status=await response.json();lastStatus=status;
      if(status.connected||status.state==="connected")return status;
      if(status.state==="failed")return status;
    }catch(error){
      setWifiConnectProgress("connecting",tr("Spa Control wisselt van netwerk…"),tr("De verbinding met het setupnetwerk kan hierbij tijdelijk wegvallen. Wacht enkele seconden."));
    }
  }
  if(lastStatus&&lastStatus.state==="failed")return lastStatus;
  return{connected:false,state:"failed",error:tr("Verbinding-time-out. Controleer het WiFi-wachtwoord en probeer opnieuw.")};
}



async function restartEsp() {
  if (!confirm(tr("Weet je zeker dat je de ESP opnieuw wilt opstarten?"))) return;

  try {
    await fetch("/api/restart", { method: "POST" });
    showToast(tr("ESP wordt opnieuw opgestart..."));

    setTimeout(() => {
      location.reload();
    }, 8000);
  } catch (error) {
    showToast(tr("Herstart mislukt."), true);
  }
}


function startOtaUpload() {
  const input = document.getElementById("otaFile");
  const file = input?.files?.[0];

  if (!file) {
    showToast(tr("Kies eerst een firmwarebestand."), true);
    return;
  }

  if (!file.name.toLowerCase().endsWith(".bin")) {
    showToast(tr("Kies een geldig .bin-bestand."), true);
    return;
  }

  const xhr = new XMLHttpRequest();
  const progress = document.getElementById("otaProgress");
  const percent = document.getElementById("otaPercent");

  progress.value = 0;
  percent.innerText = "0%";

  xhr.upload.onprogress = (e) => {
    if (!e.lengthComputable) return;

    const p = Math.round((e.loaded * 100) / e.total);

    progress.value = p;
    percent.innerText = `${p}%`;
  };

  xhr.onload = () => {
    if (xhr.status === 200) {
      progress.value = 100;
      percent.innerText = "100%";

      showToast(
        tr("Firmware geüpload. ESP start opnieuw...")
      );

      setTimeout(() => {
        location.reload();
      }, 12000);

      return;
    }

    let message = tr("OTA-update mislukt.");

    try {
      const response = JSON.parse(xhr.responseText);

      if (response?.error) {
        message = response.error;
      }
    } catch (_) {
      if (xhr.responseText) {
        message = xhr.responseText;
      }
    }

    showToast(message, true);
  };

  xhr.onerror = () => {
    showToast(
      tr("Uploadfout tijdens OTA-update."),
      true
    );
  };

  xhr.ontimeout = () => {
    showToast(
      tr("OTA-upload timeout na 180 seconden."),
      true
    );
  };

  xhr.onabort = () => {
    showToast(
      tr("OTA-upload afgebroken."),
      true
    );
  };

  xhr.open("POST", "/api/ota");
  xhr.timeout = 180000;
  xhr.setRequestHeader(
    "Content-Type",
    "application/octet-stream"
  );

  xhr.send(file);
}


function sleep(milliseconds) {
  return new Promise((resolve) => {
    setTimeout(resolve, milliseconds);
  });
}

function signalBars(rssi) {
  if (rssi >= -55) return "▮▮▮▮";
  if (rssi >= -67) return "▮▮▮▯";
  if (rssi >= -75) return "▮▮▯▯";

  return "▮▯▯▯";
}

function formatBytes(bytes) {
  const value = Number(bytes);

  if (!Number.isFinite(value)) {
    return "--";
  }

  if (value < 1024) {
    return `${value} B`;
  }

  return `${(value / 1024).toFixed(1)} KB`;
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function showToast(message, isError = false) {
  const toast =
    document.getElementById("toast");

  toast.innerText = tr(message);

  toast.className =
    "toast show" +
    (isError ? " error" : "");

  clearTimeout(toastTimer);

  toastTimer = setTimeout(() => {
    toast.className = "toast";
  }, 3500);
}

document
  .getElementById("wifiModal")
  .addEventListener("click", (event) => {
    if (event.target.id === "wifiModal") {
      closeWifiModal();
    }
  });

if ("serviceWorker" in navigator) {
  navigator.serviceWorker.register("/sw.js?v=3256");
}

loadTemperaturePreference();
connectWebSocket();



/* v3.2.0 RC3 - MQTT instellingen standaard ingeklapt */
function toggleMqttSettings(forceOpen){
  const panel=document.getElementById("mqttSettingsPanel");
  const button=document.getElementById("mqttSettingsToggle");
  if(!panel||!button)return;
  const open=typeof forceOpen==="boolean"?forceOpen:panel.hasAttribute("hidden");
  panel.toggleAttribute("hidden",!open);
  button.classList.toggle("open",open);
  button.setAttribute("aria-expanded",open?"true":"false");
}

const mqttPublicationDefaults={Temperature:"temperature",Target:"target/state",Power:"power/state",Heater:"heater/state",HeatingActive:"heater/active",Godmode:"state/godmode",FullPower:"state/fullpower",Filter:"filter/state",Bubbles:"bubbles/state",Jets:"jets/state",Locked:"lock/state",Connected:"spa/connected",Ready:"spa/ready",Rssi:"system/rssi",Heap:"system/free_heap",Uptime:"system/uptime",Firmware:"system/firmware",Ip:"system/ip",Json:"state",Maintenance:"maintenance",MaintenanceFilterReplace:"maintenance/filter_replace_days",MaintenanceFilterClean:"maintenance/filter_clean_days",MaintenanceChlorine:"maintenance/chlorine_days"};
async function loadMqttSettings(){try{const r=await fetch("/api/settings/mqtt",{cache:"no-store"});if(!r.ok)throw new Error("MQTT-instellingen niet beschikbaar");const s=await r.json();const v=(i,x)=>{const e=document.getElementById(i);if(e)e.value=x??""};const c=(i,x)=>{const e=document.getElementById(i);if(e)e.checked=!!x};c("mqttEnabled",s.enabled);v("mqttHost",s.host);v("mqttPort",s.port);v("mqttUser",s.username);v("mqttClientId",s.clientId);v("mqttBaseTopic",s.baseTopic);c("mqttDiscovery",s.homeAssistantDiscovery);Object.entries(mqttPublicationDefaults).forEach(([key,def])=>{c(`publish${key}`,s[`publish${key}`]);v(`topic${key}`,s[`topic${key}`]||def);});updateMqttPublicationCards();}catch(e){showToast(e.message,true);}}
function updateMqttPublicationCards(){Object.keys(mqttPublicationDefaults).forEach(key=>{const input=document.getElementById(`publish${key}`);input?.closest('.mqtt-publication-card')?.classList.toggle('disabled',!input.checked);});}
function resetMqttPublicationDefaults(){Object.entries(mqttPublicationDefaults).forEach(([key,value])=>{const input=document.getElementById(`topic${key}`);if(input)input.value=value;});showToast(tr("Standaardtopics hersteld"));}
async function saveMqttSettings(){const values={enabled:document.getElementById("mqttEnabled")?.checked?"true":"false",host:document.getElementById("mqttHost")?.value||"",port:document.getElementById("mqttPort")?.value||"1883",username:document.getElementById("mqttUser")?.value||"",password:document.getElementById("mqttPassword")?.value||"",clientId:document.getElementById("mqttClientId")?.value||"LayZSpaController",baseTopic:document.getElementById("mqttBaseTopic")?.value||"layzspa",homeAssistantDiscovery:document.getElementById("mqttDiscovery")?.checked?"true":"false"};Object.keys(mqttPublicationDefaults).forEach(key=>{values[`publish${key}`]=document.getElementById(`publish${key}`)?.checked?"true":"false";values[`topic${key}`]=document.getElementById(`topic${key}`)?.value||mqttPublicationDefaults[key];});const b=new URLSearchParams(values);try{const r=await fetch("/api/settings/mqtt",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:b.toString()});if(!r.ok)throw new Error(tr("Opslaan mislukt"));updateMqttPublicationCards();showToast(tr("MQTT-instellingen opgeslagen"));}catch(e){showToast(e.message,true);}}
document.addEventListener("change",e=>{if(e.target?.id?.startsWith("publish"))updateMqttPublicationCards();});

let maintenanceData=null;
function maintenanceLabel(item){if(!item.enabled)return tr("Uit");if(item.due)return item.daysRemaining<0?`${Math.abs(item.daysRemaining)} ${tr("dagen te laat")}`:tr("Vandaag nodig");if(item.daysRemaining<=2)return `${tr("Over")} ${item.daysRemaining} ${tr(item.daysRemaining===1?"dag":"dagen")}`;return `${item.daysRemaining} ${tr("dagen")}`;}
function renderMaintenance(data){maintenanceData=data;const alertTexts={filterReplace:{due:"Filter vervangen nodig",soon:"Filter vervangen binnenkort"},filterClean:{due:"Filter schoonmaken nodig",soon:"Filter schoonmaken binnenkort"},chlorine:{due:"Chloor toevoegen nodig",soon:"Chloor toevoegen binnenkort"}};let alerts=[];Object.entries(alertTexts).forEach(([key,texts])=>{const item=data[key];const card=document.querySelector(`[data-maintenance-card="${key}"]`);if(!card||!item)return;card.classList.toggle("due",item.enabled&&item.due);card.classList.toggle("soon",item.enabled&&!item.due&&item.daysRemaining<=2);card.classList.toggle("disabled",!item.enabled);card.querySelector('[data-role="last"]').textContent=item.lastDate||tr("Nog niet geregistreerd");card.querySelector('[data-role="next"]').textContent=item.nextDate||tr("Na eerste registratie");card.querySelector('.maintenance-badge').textContent=maintenanceLabel(item);const en=document.getElementById(`${key}Enabled`),days=document.getElementById(`${key}Days`);if(en)en.checked=item.enabled;if(days)days.value=item.intervalDays;if(item.enabled&&item.due)alerts.push(`🔴 ${tr(texts.due)}`);else if(item.enabled&&item.daysRemaining<=2)alerts.push(`🟠 ${tr(texts.soon)}`);});const overall=document.getElementById("maintenanceOverall");if(overall){overall.textContent=data.status==="ALARM"?tr("Onderhoud nodig"):data.status==="WARNING"?tr("Binnenkort onderhoud"):tr("Alles in orde");overall.dataset.status=data.status;}const alert=document.getElementById("maintenanceAlert");if(alert){alert.classList.toggle("hidden",alerts.length===0);alert.innerHTML=alerts.length?`<strong>${tr("Onderhoudsmelding")}</strong><span>${alerts.join("<br>")}</span>`:"";}renderDashboardMaintenanceAlert(data);}
function renderDashboardMaintenanceAlert(data){let box=document.getElementById("dashboardMaintenanceAlert");if(!box){const dashboard=document.getElementById("dashboardView");const head=dashboard?.querySelector('.page-head');if(dashboard&&head){box=document.createElement('button');box.id="dashboardMaintenanceAlert";box.className="dashboard-maintenance-alert hidden";box.onclick=()=>showView('maintenance');head.insertAdjacentElement('afterend',box);}}if(!box)return;const needed=data.status!=="OK";box.classList.toggle("hidden",!needed);box.textContent=data.status==="ALARM"?tr("🧰 Onderhoud nodig – tik om te bekijken"):tr("🧰 Onderhoud binnenkort – tik om te bekijken");}
async function loadMaintenance(){try{const r=await fetch('/api/maintenance',{cache:'no-store'});if(!r.ok)throw new Error(tr('Onderhoud kon niet worden geladen'));renderMaintenance(await r.json());}catch(e){showToast(e.message,true);}}
async function saveMaintenanceSettings(){const b=new URLSearchParams({filterReplaceEnabled:document.getElementById('filterReplaceEnabled')?.checked?'true':'false',filterReplaceDays:document.getElementById('filterReplaceDays')?.value||'30',filterCleanEnabled:document.getElementById('filterCleanEnabled')?.checked?'true':'false',filterCleanDays:document.getElementById('filterCleanDays')?.value||'7',chlorineEnabled:document.getElementById('chlorineEnabled')?.checked?'true':'false',chlorineDays:document.getElementById('chlorineDays')?.value||'3'});try{const r=await fetch('/api/maintenance',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:b.toString()});if(!r.ok)throw new Error(tr('Onderhoudsinstellingen opslaan mislukt'));renderMaintenance(await r.json());showToast(tr('Onderhoudsinstellingen opgeslagen'));}catch(e){showToast(e.message,true);}}
async function markMaintenanceDone(item){try{const r=await fetch('/api/maintenance/done',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded;charset=UTF-8'},body:new URLSearchParams({item}).toString()});const result=await r.json();if(!r.ok)throw new Error(tr(result.error||'Registreren mislukt'));renderMaintenance(result);showToast(tr('Datum bijgewerkt naar vandaag'));}catch(e){showToast(e.message,true);}}

async function loadRegionalSettings(){
  try{
    const r=await fetch("/api/settings/region",{cache:"no-store"});
    if(!r.ok) throw new Error("Regionale instellingen niet beschikbaar");
    const s=await r.json();
    const set=(id,val)=>{const e=document.getElementById(id); if(e) e.value=val??"";};
    const check=(id,val)=>{const e=document.getElementById(id); if(e) e.checked=!!val;};
    set("timeZone",s.timeZone||"Europe/Amsterdam");
    set("dateFormat",s.dateFormat||"DD-MM-YYYY");
    check("clock24",s.use24HourClock!==false);
  }catch(e){showToast(e.message,true);}
}

async function saveRegionalSettings(){
  saveWeatherSettings();
  const body=new URLSearchParams({
    timeZone:document.getElementById("timeZone")?.value||"Europe/Amsterdam",
    dateFormat:document.getElementById("dateFormat")?.value||"DD-MM-YYYY",
    use24HourClock:document.getElementById("clock24")?.checked?"true":"false"
  });
  try{
    const r=await fetch("/api/settings/region",{
      method:"POST",
      headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},
      body:body.toString()
    });
    if(!r.ok) throw new Error(tr("Opslaan mislukt"));
    render();
    showToast(tr("Regionale instellingen opgeslagen"));
  }catch(e){showToast(e.message,true);}
}


let historyData = [];
let logData = [];

function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  localStorage.setItem("spaTheme", theme);
  const button = document.getElementById("themeToggle");
  if (button) button.textContent = theme === "light" ? "🌙" : "☀️";
}

function toggleTheme() {
  applyTheme(document.documentElement.dataset.theme === "light" ? "dark" : "light");
}

function initTheme() {
  const stored = localStorage.getItem("spaTheme");
  const preferred = window.matchMedia && window.matchMedia("(prefers-color-scheme: light)").matches ? "light" : "dark";
  applyTheme(stored || preferred);
}

async function loadSystemStatus() {
  try {
    const response = await fetch("/api/settings/system", { cache: "no-store" });
    if (!response.ok) return;
    const data = await response.json();
    setText("dashboardClock", data.time || "--:--");
    setText("dashboardDate", data.date || "--");
    setText("dashboardTimeSync", tr(data.timeSynchronized ? "Gesynchroniseerd" : "Wachten"));
    setText("dashboardTimeZone", data.timeZone || "--");
  } catch (_) {}
}

async function loadHistory() {
  const limit = document.getElementById("historyLimit")?.value || "288";
  try {
    const response = await fetch(`/api/history?limit=${encodeURIComponent(limit)}`, { cache: "no-store" });
    if (!response.ok) throw new Error("Historie niet beschikbaar");
    historyData = await response.json();
    renderHistory();
  } catch (error) {
    showToast(error.message, true);
  }
}

function renderHistory() {
  const empty = document.getElementById("historyEmpty");
  const canvas = document.getElementById("temperatureChart");
  if (!canvas) return;
  const values = historyData.map(item => Number(item.temperature)).filter(Number.isFinite);
  empty?.classList.toggle("hidden", values.length > 0);
  canvas.classList.toggle("hidden", values.length === 0);
  setText("historyCount", historyData.length);
  setText("historyMin", values.length ? `${displayTemperature(Math.min(...values))} ${temperatureSymbol()}` : "--");
  setText("historyAvg", values.length ? `${displayTemperature(values.reduce((a,b)=>a+b,0)/values.length)} ${temperatureSymbol()}` : "--");
  setText("historyMax", values.length ? `${displayTemperature(Math.max(...values))} ${temperatureSymbol()}` : "--");
  drawTemperatureChart(canvas, historyData);
  renderActivityTimeline();
}

function drawTemperatureChart(canvas, data) {
  if (!data.length) return;
  const ratio = window.devicePixelRatio || 1;
  const width = canvas.clientWidth || 600;
  const height = 260;
  canvas.width = width * ratio;
  canvas.height = height * ratio;
  const ctx = canvas.getContext("2d");
  ctx.scale(ratio, ratio);
  ctx.clearRect(0, 0, width, height);
  const pad = { left: 42, right: 14, top: 18, bottom: 32 };
  const all = data.flatMap(item => [Number(item.temperature), Number(item.target)]).filter(Number.isFinite);
  let min = Math.floor(Math.min(...all) - 1), max = Math.ceil(Math.max(...all) + 1);
  if (max - min < 4) max = min + 4;
  const x = i => pad.left + (data.length === 1 ? 0 : i * (width-pad.left-pad.right)/(data.length-1));
  const y = value => pad.top + (max-value)*(height-pad.top-pad.bottom)/(max-min);
  ctx.font = "12px system-ui";
  ctx.strokeStyle = getComputedStyle(document.documentElement).getPropertyValue("--line").trim() || "rgba(255,255,255,.15)";
  ctx.fillStyle = getComputedStyle(document.documentElement).getPropertyValue("--muted").trim() || "#8fa8bf";
  for (let i=0;i<=4;i++) { const v=min+(max-min)*i/4; const yy=y(v); ctx.beginPath(); ctx.moveTo(pad.left,yy); ctx.lineTo(width-pad.right,yy); ctx.stroke(); ctx.fillText(`${v.toFixed(0)}°`,4,yy+4); }
  const drawLine=(key,style)=>{ ctx.strokeStyle=style; ctx.lineWidth=2.5; ctx.beginPath(); data.forEach((item,i)=>{ const value=Number(item[key]); if(!Number.isFinite(value)) return; const xx=x(i), yy=y(value); i===0?ctx.moveTo(xx,yy):ctx.lineTo(xx,yy); }); ctx.stroke(); };
  drawLine("target", "#ff9f43");
  drawLine("temperature", "#19a8ff");
  const first = new Date(Number(data[0].timestamp)*1000); const last = new Date(Number(data[data.length-1].timestamp)*1000);
  ctx.fillText(first.toLocaleTimeString([], {hour:"2-digit",minute:"2-digit"}), pad.left, height-8);
  const label=last.toLocaleTimeString([], {hour:"2-digit",minute:"2-digit"}); ctx.fillText(label, width-pad.right-ctx.measureText(label).width, height-8);
}

function renderActivityTimeline() {
  const container = document.getElementById("activityTimeline");
  if (!container) return;
  if (!historyData.length) { container.innerHTML = '<div class="empty-state">Nog geen activiteit.</div>'; return; }
  const keys=[['heater','🔥'],['filter','💧'],['bubbles','🫧'],['jets','🌊']];
  container.innerHTML = keys.map(([key,icon]) => `<div class="activity-row"><span>${icon} ${key}</span><div class="activity-bar">${historyData.map(item=>`<i class="${item[key]?'on':''}"></i>`).join('')}</div></div>`).join('');
}

async function clearHistory() {
  if (!confirm(tr("Historie volledig wissen?"))) return;
  const response = await fetch("/api/history", { method: "DELETE" });
  if (response.ok) { historyData=[]; renderHistory(); showToast(tr("Historie gewist")); }
}

async function loadEnergy() {
  try {
    const energyResponse = await fetch("/api/energy", { cache: "no-store" });
    if (!energyResponse.ok) throw new Error("Energiegegevens niet beschikbaar");
    const data = await energyResponse.json();
    setText("energyTotalKwh", `${Number(data.totalKwh || 0).toFixed(3)} kWh`);
    setText("energyTotalCost", `${data.currency || "€"} ${Number(data.estimatedCost || 0).toFixed(2)}`);
    setText("dashboardKwh", `${Number(data.totalKwh || 0).toFixed(2)} kWh`);
    setText("dashboardCost", `${data.currency || "€"} ${Number(data.estimatedCost || 0).toFixed(2)}`);
    setText("energyHeaterHours", `${Number(data.heaterHours || 0).toFixed(2)} u`);
    setText("energyFilterHours", `${Number(data.filterHours || 0).toFixed(2)} u`);
    setText("energyBubblesHours", `${Number(data.bubblesHours || 0).toFixed(2)} u`);
    setText("energyJetsHours", `${Number(data.jetsHours || 0).toFixed(2)} u`);
    [["energyHeaterWatts",data.heaterWatts],["energyFilterWatts",data.filterWatts],["energyBubblesWatts",data.bubblesWatts],["energyJetsWatts",data.jetsWatts],["energyPrice",data.pricePerKwh],["energyCurrency",data.currency]].forEach(([id,v])=>{const e=document.getElementById(id);if(e)e.value=v??"";});
  } catch (error) { showToast(error.message, true); }
}

async function saveEnergySettings() {
  const body = new URLSearchParams({
    heaterWatts: document.getElementById("energyHeaterWatts")?.value || "2200",
    filterWatts: document.getElementById("energyFilterWatts")?.value || "60",
    bubblesWatts: document.getElementById("energyBubblesWatts")?.value || "800",
    jetsWatts: document.getElementById("energyJetsWatts")?.value || "800",
    pricePerKwh: document.getElementById("energyPrice")?.value || "0.30",
    currency: document.getElementById("energyCurrency")?.value || "€"
  });
  const response = await fetch("/api/energy", {method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:body.toString()});
  if (response.ok) { showToast(tr("Energie-instellingen opgeslagen")); loadEnergy(); } else showToast(tr("Opslaan mislukt"), true);
}

async function resetEnergy() {
  if (!confirm(tr("Alle energietellers resetten?"))) return;
  const response=await fetch("/api/energy",{method:"DELETE"});
  if(response.ok){showToast(tr("Energietellers gereset"));loadEnergy();}
}



let schedulesData=[];
const plannerDayDefinitions=[
  {mask:2,id:"maandag",label:"Maandag"},
  {mask:4,id:"dinsdag",label:"Dinsdag"},
  {mask:8,id:"woensdag",label:"Woensdag"},
  {mask:16,id:"donderdag",label:"Donderdag"},
  {mask:32,id:"vrijdag",label:"Vrijdag"},
  {mask:64,id:"zaterdag",label:"Zaterdag"},
  {mask:1,id:"zondag",label:"Zondag"}
];
const plannerActionLabels=["Heater aan","Heater uit","Filter aan","Filter uit","Bubbels aan","Bubbels uit","Jets aan","Jets uit","Doeltemperatuur"];

function openScheduleModal(){
  const modal=document.getElementById("scheduleModal");
  if(!modal){showToast(tr("Planner-venster ontbreekt"),true);return;}
  document.getElementById("schTime").value="08:00";
  document.getElementById("schAction").value="0";
  document.getElementById("schTemp").value="38";
  document.querySelectorAll('#schDays input[type="checkbox"]').forEach(input=>{input.checked=false;});
  modal.classList.add("open");
  modal.setAttribute("aria-hidden","false");
}
function closeScheduleModal(){
  const modal=document.getElementById("scheduleModal");
  if(!modal)return;
  modal.classList.remove("open");
  modal.setAttribute("aria-hidden","true");
}
function plannerActionText(item){
  const label=tr(plannerActionLabels[Number(item.action)]||"Onbekend");
  return Number(item.action)===8?`${label}: ${Number(item.value)||0}°`:label;
}
function renderSchedules(){
  for(const day of plannerDayDefinitions){
    const list=document.getElementById(`day-${day.id}`);
    if(!list)continue;
    const rows=schedulesData.filter(item=>(Number(item.daysMask)&day.mask)!==0);
    if(!rows.length){list.innerHTML=`<em>${escapeHtml(tr("Nog geen schema's"))}</em>`;continue;}
    list.innerHTML=rows.map(item=>{
      const time=`${String(item.hour).padStart(2,"0")}:${String(item.minute).padStart(2,"0")}`;
      return `<article class="schedule-row" data-schedule-id="${Number(item.id)}">
        <label class="schedule-select"><input type="checkbox" data-schedule-select data-schedule-id="${Number(item.id)}" data-day-mask="${Number(day.mask)}" aria-label="${escapeHtml(tr("Schema selecteren"))}"></label>
        <strong>${time}</strong>
        <span>${escapeHtml(plannerActionText(item))}</span>
        <label class="ui-switch" title="${escapeHtml(tr("Ingeschakeld"))}"><input type="checkbox" ${item.enabled!==false?"checked":""} onchange="setScheduleEnabled(${Number(item.id)},${Number(day.mask)},this.checked)"><i></i></label>
        <button class="icon-action schedule-delete" type="button" onclick="deleteScheduleDay(${Number(item.id)},${Number(day.mask)})" title="${escapeHtml(tr("Verwijderen"))}">🗑️</button>
      </article>`;
    }).join("");
  }
  translateDom(document.getElementById("plannerView"));
}
async function loadSchedules(){
  try{
    const response=await fetch("/api/schedules",{cache:"no-store"});
    if(!response.ok)throw new Error(tr("Planner kon niet worden geladen"));
    schedulesData=await response.json();
    if(!Array.isArray(schedulesData))schedulesData=[];
    renderSchedules();
  }catch(error){showToast(error.message||tr("Planner kon niet worden geladen"),true);}
}
async function saveSchedule(){
  const time=(document.getElementById("schTime")?.value||"").split(":");
  const checked=[...document.querySelectorAll('#schDays input[type="checkbox"]:checked')];
  const daysMask=checked.reduce((sum,input)=>sum+Number(input.value||0),0);
  if(time.length!==2||daysMask===0){showToast(tr("Kies een tijd en minimaal één dag"),true);return;}
  const action=Number(document.getElementById("schAction")?.value||0);
  const values={enabled:"true",daysMask:String(daysMask),hour:String(Number(time[0])),minute:String(Number(time[1])),action:String(action),value:String(Number(document.getElementById("schTemp")?.value||38))};
  try{
    const response=await fetch("/api/schedules",{method:"POST",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:new URLSearchParams(values).toString()});
    const result=await response.json().catch(()=>({}));
    if(!response.ok)throw new Error(tr(result.error||"Schema niet opgeslagen"));
    closeScheduleModal();
    showToast(tr("Schema opgeslagen"));
    await loadSchedules();
  }catch(error){showToast(error.message||tr("Schema niet opgeslagen"),true);}
}
let scheduleDayToggleBusy=false;
async function setScheduleEnabled(id,dayMask,enabled){
  if(scheduleDayToggleBusy){renderSchedules();return;}
  if(!schedulesData.some(item=>Number(item.id)===Number(id)))return;
  scheduleDayToggleBusy=true;
  // Show the confirmed state until the server has saved and returned the new rows.
  renderSchedules();
  document.querySelectorAll('input[onchange^="setScheduleEnabled("]').forEach(input=>input.disabled=true);
  try{
    const values={id:String(id),dayMask:String(dayMask),enabled:enabled?"true":"false"};
    const response=await fetch("/api/schedules",{method:"PUT",headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:new URLSearchParams(values).toString()});
    const result=await response.json().catch(()=>({}));
    if(!response.ok)throw new Error(tr(result.error||"Schema niet bijgewerkt"));
    await loadSchedules();
  }catch(error){showToast(error.message,true);await loadSchedules();}
  finally{
    scheduleDayToggleBusy=false;
    document.querySelectorAll('input[onchange^="setScheduleEnabled("]').forEach(input=>input.disabled=false);
  }
}
async function deleteScheduleDay(id,dayMask){
  if(!confirm(tr("Schema verwijderen?")))return false;

  try{
    const response=await fetch(
      `/api/schedules?id=${encodeURIComponent(String(id))}&dayMask=${encodeURIComponent(String(dayMask))}`,
      {method:"DELETE",cache:"no-store"}
    );

    if(!response.ok){
      const result=await response.json().catch(()=>({}));
      throw new Error(tr(result.error||"Schema verwijderen mislukt"));
    }

    showToast(tr("Schema's verwijderd"));
    await loadSchedules();
    return true;
  }catch(error){
    showToast(error.message||tr("Schema verwijderen mislukt"),true);
    return false;
  }
}

async function deleteSchedule(id,ask=true){
  if(ask&&!confirm(tr("Schema verwijderen?")))return false;
  const response=await fetch(`/api/schedules?id=${encodeURIComponent(String(id))}`,{method:"DELETE",cache:"no-store"});
  if(!response.ok){showToast(tr("Schema verwijderen mislukt"),true);return false;}
  if(ask){
    showToast(tr("Schema's verwijderd"));
    await loadSchedules();
  }
  return true;
}
async function deleteSelectedSchedules(){
  const selected=[...document.querySelectorAll('[data-schedule-select]:checked')].map(input=>({
    id:Number(input.dataset.scheduleId),
    dayMask:Number(input.dataset.dayMask)
  }));

  if(!selected.length){
    showToast(tr("Selecteer minimaal één schema"),true);
    return;
  }

  if(!confirm(tr("Aangevinkte schema's verwijderen?")))return;

  try{
    for(const item of selected){
      const response=await fetch(
        `/api/schedules?id=${encodeURIComponent(String(item.id))}&dayMask=${encodeURIComponent(String(item.dayMask))}`,
        {method:"DELETE",cache:"no-store"}
      );

      if(!response.ok){
        const result=await response.json().catch(()=>({}));
        throw new Error(tr(result.error||"Schema verwijderen mislukt"));
      }
    }

    showToast(tr("Schema's verwijderd"));
    await loadSchedules();

  }catch(error){
    showToast(error.message||tr("Schema verwijderen mislukt"),true);
    await loadSchedules();
  }
}

function translateLogMessage(message){
  const raw=String(message||"");
  const exact=tr(raw);
  if(exact!==raw) return exact;
  const patterns=[
    [/Scheduler geladen: (\d+) schema's/i,(_,n)=>`${tr("Planner geladen")}: ${n} ${tr("schema's")}`],
    [/MQTT verbindingspoging mislukt.*code[=: ](-?\d+)/i,(_,n)=>`${tr("MQTT connection attempt failed")} (${tr("status")} ${n})`],
    [/Hardwareconfiguratie opgeslagen; herstart vereist/i,()=>tr("Hardware configuration saved; restart required")]
  ];
  for(const [pattern,replacement] of patterns)if(pattern.test(raw))return raw.replace(pattern,replacement);
  return raw;
}
let liveLogTimer = null;
let debugLoggingEnabled = false;
let debugLoggingKnown = false;
let loggingRequestVersion = 0;
let loggingUpdatePending = false;
let loggingLastUptimeMs = null;

function scheduleLiveLogRefresh() {
  if (liveLogTimer) clearInterval(liveLogTimer);
  liveLogTimer = null;
  if (!debugLoggingEnabled || !debugLoggingKnown) return;
  liveLogTimer = setInterval(() => {
    if (document.getElementById("logsView")?.classList.contains("active")) {
      loadDiagnostics(); loadLogs(true);
    }
  }, 2000);
}

function applyLoggingState(data) {
  if (typeof data.enabled !== "boolean") throw new Error("Ongeldige loggingstatus");
  if (Number.isFinite(data.uptimeMs)) {
    if (loggingLastUptimeMs !== null && data.uptimeMs < loggingLastUptimeMs) {
      showToast(tr("ESP herstart: debug logging staat weer standaard UIT"));
    }
    loggingLastUptimeMs = data.uptimeMs;
  }
  debugLoggingEnabled = data.enabled;
  debugLoggingKnown = true;
  const checkbox = document.getElementById("liveLogEnabled");
  if (checkbox) checkbox.checked = debugLoggingEnabled;
  setText("liveLogStateText", debugLoggingEnabled ? "AAN · bevestigd door ESP" : "UIT · bevestigd door ESP");
  scheduleLiveLogRefresh();
}

function loggingStatusUnavailable() {
  const checkbox = document.getElementById("liveLogEnabled");
  if (checkbox) checkbox.checked = debugLoggingEnabled;
  setText("liveLogStateText", debugLoggingKnown
    ? `Status niet bereikbaar · laatst bevestigd ${debugLoggingEnabled ? 'AAN' : 'UIT'}`
    : "Status nog niet bevestigd");
}

async function loadLoggingState() {
  if (loggingUpdatePending) return debugLoggingKnown && debugLoggingEnabled;
  const version = ++loggingRequestVersion;
  try {
    const response = await fetch("/api/logging", {cache:"no-store", signal:AbortSignal.timeout(7000)});
    if (!response.ok) throw new Error("Loggingstatus ophalen mislukt");
    const data = await response.json();
    if (version !== loggingRequestVersion || loggingUpdatePending) return debugLoggingKnown && debugLoggingEnabled;
    applyLoggingState(data);
  } catch (_) {
    if (version === loggingRequestVersion && !loggingUpdatePending) loggingStatusUnavailable();
  }
  return debugLoggingKnown && debugLoggingEnabled;
}

async function updateLiveLogState() {
  if (loggingUpdatePending) return;
  const checkbox = document.getElementById("liveLogEnabled");
  const enabled = !!checkbox?.checked;
  loggingUpdatePending = true;
  ++loggingRequestVersion; // An older GET must never overwrite this user's change.
  if (checkbox) checkbox.disabled = true;
  setText("liveLogStateText", "Instelling opslaan…");
  let failed = false;
  try {
    const response = await fetch("/api/logging", {
      method:"POST",
      headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},
      body:new URLSearchParams({enabled:enabled ? "true" : "false"}).toString(),
      signal:AbortSignal.timeout(7000)
    });
    if (!response.ok) throw new Error((await response.text()) || tr("Logging-instelling opslaan mislukt"));
    const data = await response.json();
    applyLoggingState(data);
  } catch (error) {
    failed = true;
    loggingStatusUnavailable();
    showToast(error.name === "TimeoutError" ? "Geen antwoord op logging-instelling; serverstatus wordt gecontroleerd" : error.message, true);
  } finally {
    loggingUpdatePending = false;
    if (checkbox) checkbox.disabled = false;
  }
  // A timed-out POST may still have reached the ESP. One GET reconciles it.
  if (failed) await loadLoggingState();
  if (debugLoggingKnown && debugLoggingEnabled) await loadLogs(true);
  else if (debugLoggingKnown) { logData = []; renderLogs(); }
}

async function loadLogs(silent = false) {
  if (!debugLoggingEnabled) {
    logData = [];
    renderLogs();
    return;
  }

  try {
    const response = await fetch("/api/logs?limit=50", {cache:"no-store"});
    if (!response.ok) throw new Error(tr("Logboek niet beschikbaar"));
    logData = await response.json();
    renderLogs();
  } catch (error) {
    if (!silent) showToast(error.message, true);
  }
}

function renderLogs() {
  const list=document.getElementById("logList"); if(!list)return;
  const filter=document.getElementById("logFilter")?.value||"ALL";
  const rows=logData.filter(item=>filter==="ALL"||item.level===filter).reverse();
  if(!rows.length){list.innerHTML=`<div class="empty-state">${tr("Geen logregels.")}</div>`;return;}
  list.innerHTML=rows.map(item=>{const date=new Date(Number(item.timestamp)*1000);return `<article class="log-entry level-${String(item.level).toLowerCase()}"><time>${date.toLocaleString()}</time><b>${escapeHtml(tr(item.level))}</b><p>${escapeHtml(translateLogMessage(item.message))}</p></article>`;}).join('');
}

async function clearLogs(){if(!confirm(tr("Logboek wissen?")))return;const r=await fetch("/api/logs",{method:"DELETE"});if(r.ok){logData=[];renderLogs();showToast(tr("Logboek gewist"));}}

window.addEventListener("resize", () => { if (document.getElementById("historyView")?.classList.contains("active")) renderHistory(); });
initTheme();
const liveLogCheckbox = document.getElementById("liveLogEnabled");
if (liveLogCheckbox) liveLogCheckbox.checked = false;
// Server start altijd met debug logging UIT. Alleen status uitlezen; nooit automatisch inschakelen.
loadLoggingState();


let diagnosticData = {};

function diagnosticState(value) {
  return value ? "OK" : tr("Aandacht");
}

async function loadDiagnostics() {
  try {
    const response = await fetch("/api/diagnostics", { cache: "no-store" });
    if (!response.ok) throw new Error(tr("Diagnostiek niet beschikbaar"));
    diagnosticData = await response.json();
    setText("diagWifi", diagnosticState(diagnosticData.wifiConnected));
    setText("diagRssi", diagnosticData.wifiConnected ? `${diagnosticData.rssi} dBm` : "Offline");
    setText("diagMqtt", diagnosticState(diagnosticData.mqttConnected));
    setText("diagTime", diagnosticState(diagnosticData.timeSynchronized));
    setText("diagSpa", diagnosticState(diagnosticData.spaConnected));
    const otaError = String(diagnosticData.otaLastError || "");
    const mqttError = Number(diagnosticData.mqttLastError || 0);
    setText("diagOta", otaError ? tr("Aandacht") : "OK");
    setText("diagOtaError", otaError || tr("Geen fout gemeld"));
    setText("diagMqttReconnects", String(diagnosticData.mqttReconnectCount || 0));
    setText("diagMqttError", mqttError ? `MQTT status ${mqttError}` : tr("Geen fout gemeld"));
    setText("diagSpaData", diagnosticData.spaDataValid ? tr("Data geldig") : tr("Geen geldige data"));
    setText("diagHeap", formatBytes(diagnosticData.freeHeap || 0));
    setText("diagFragmentation", `${diagnosticData.heapFragmentation || 0}% ${tr("fragmentatie")}`);
    const ledAvailable = diagnosticData.statusLedAvailable !== false;
    const ledColor = String(diagnosticData.statusLedColor || "--");
    const ledState = String(diagnosticData.statusLedState || "--");
    setText("diagStatusLed", ledAvailable ? ledColor : tr("Niet beschikbaar"));
    setText("diagStatusLedDetail", ledAvailable
      ? `GPIO${diagnosticData.statusLedGpio ?? 8} · ${ledState}${diagnosticData.statusLedBlinking ? ` · ${tr("knippert")}` : ""}`
      : `GPIO${diagnosticData.statusLedGpio ?? 8}`);
  } catch (error) { showToast(error.message, true); }
}

async function runSelfTest() {
  try {
    const response = await fetch("/api/selftest", { method: "POST" });
    const result = await response.json();
    const labels = {memory:"Geheugen",wifi:"WiFi",time:"Tijd",mqtt:"MQTT",spa:"Spa-data"};
    const summary = Object.entries(labels).map(([key,label])=>`${result[key]?"✓":"⚠"} ${label}`).join(" · ");
    showToast(summary, !result.ok);
    await loadDiagnostics();
    if (debugLoggingEnabled) await loadLogs();
  } catch (error) { showToast(tr("Zelftest mislukt"), true); }
}

function downloadTextFile(filename, text, type="text/plain") {
  const blob = new Blob([text], { type });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url; link.download = filename; link.click();
  setTimeout(()=>URL.revokeObjectURL(url), 1000);
}

async function downloadDiagnosticReport() {
  try {
    const diagResponse = await fetch("/api/diagnostics", {cache:"no-store", signal:AbortSignal.timeout(7000)});
    if (!diagResponse.ok) throw new Error("Diagnostiek ophalen mislukt");
    const diag = await diagResponse.json();
    let loggingState = null;
    let logLines = [];
    try {
      const stateResponse = await fetch("/api/logging", {cache:"no-store", signal:AbortSignal.timeout(7000)});
      if (!stateResponse.ok) throw new Error("Loggingstatus ophalen mislukt");
      loggingState = await stateResponse.json();
      if (typeof loggingState.enabled !== "boolean") throw new Error("Ongeldige loggingstatus");
    } catch (_) {
      loggingState = null;
      logLines = ["Loggingstatus niet bereikbaar; niet vastgesteld of logging AAN of UIT staat"];
    }
    if (loggingState) {
      if (loggingState.enabled) {
        try {
          const logsResponse = await fetch("/api/logs?limit=50", {cache:"no-store", signal:AbortSignal.timeout(7000)});
          if (!logsResponse.ok) throw new Error("Logregels ophalen mislukt");
          const logs = await logsResponse.json();
          logLines = logs.length ? logs.map(item=>`${item.timestamp} [${item.level}] ${item.message}`) : ["Logging AAN; geen logregels aanwezig"];
        } catch (_) { logLines = ["Logging AAN; logregels konden niet worden opgehaald"]; }
      } else { logLines = ["Debug logging UIT volgens ESP"]; }
    }
    const report = [
      "Bestway Lay-Z-Spa diagnostisch rapport v0.5",
      `Gegenereerd: ${new Date().toLocaleString()}`, "",
      JSON.stringify(diag, null, 2), "", "Loggingstatus volgens ESP:",
      JSON.stringify(loggingState, null, 2), "", "Laatste logregels:", ...logLines
    ].join("\n");
    downloadTextFile(`layzspa-diagnostics-${Date.now()}.txt`, report);
  } catch (error) { showToast(error.message || tr("Rapport maken mislukt"), true); }
}

async function fetchJsonOrNull(url) {
  const response = await fetch(url, {cache:"no-store"});
  if (response.status === 404) return null;
  if (!response.ok) throw new Error(url);
  return response.json();
}

async function fetchBackupFile(name) {
  const response = await fetch(`/api/backup/file?name=${encodeURIComponent(name)}`, {cache:"no-store"});
  if (response.status === 404) return null;
  if (!response.ok) throw new Error(`backup:${name}`);
  return response.json();
}

function removeLegacyAutomationReferences(key, value) {
  if (typeof value !== "string" || !value.includes("automation")) return value;
  try {
    const parsed = JSON.parse(value);
    if (Array.isArray(parsed)) return JSON.stringify(parsed.filter(item => item !== "automation"));
    if (parsed && typeof parsed === "object") {
      delete parsed.automation;
      return JSON.stringify(parsed);
    }
  } catch (_) {}
  return value;
}

function exportBrowserSettings() {
  const keys = [
    "layzspaTemperatureUnit","layzspaDashboardOrder","layzspaDashboardButtonOrder",
    "layzspaNavigationOrder","layzspaBottomNavEnabled","layzspaNavTabs",
    "layzspaDashboardSections","layzspaDashboardButtons","spaTheme",
    "layzspaWeatherSettings","layzspaLanguageMode","layzspaLanguage",
    "layzspaVisualStyle","layzspaAccent","layzspaMotion","layzspaCompact","layzspaSmartInsight"
  ];
  const values = {};
  keys.forEach(key => {
    const value = localStorage.getItem(key);
    if (value !== null) values[key] = removeLegacyAutomationReferences(key, value);
  });
  return values;
}

async function exportBackup() {
  try {
    const [settings, hardware, maintenance, schedules, energy] = await Promise.all([
      fetchBackupFile("settings"), fetchBackupFile("hardware"),
      fetchBackupFile("maintenance"), fetchJsonOrNull("/api/schedules"), fetchJsonOrNull("/api/energy")
    ]);
    const backup = {
      format:"SpaControlBackup", version:"2.0", backupVersion:2,
      createdAt:new Date().toISOString(), wifiIncluded:false,
      files:{settings,hardware,maintenance},
      schedules:schedules||[], energy:energy||null,
      browser:exportBrowserSettings()
    };
    downloadTextFile(`spa-control-backup-${Date.now()}.json`, JSON.stringify(backup,null,2), "application/json");
    showToast(tr("Back-up gedownload"));
  } catch (error) { showToast(tr("Back-up maken mislukt"), true); }
}

async function postForm(url, values, method="POST") {
  const body = new URLSearchParams();
  Object.entries(values || {}).forEach(([key,value])=>{
    if (value !== undefined && value !== null && typeof value !== "object") body.set(key,String(value));
  });
  const response = await fetch(url,{method,headers:{"Content-Type":"application/x-www-form-urlencoded;charset=UTF-8"},body:body.toString()});
  if(!response.ok) throw new Error(url);
  return response;
}

async function restoreBackupFile(name, value) {
  if (!value) return;
  const response = await fetch(`/api/backup/file?name=${encodeURIComponent(name)}`, {
    method:"POST", headers:{"Content-Type":"application/json"}, body:JSON.stringify(value)
  });
  if (!response.ok) {
    let message = "";
    try { message = (await response.json()).error || ""; } catch (_) {}
    throw new Error(message || `${name}: HTTP ${response.status}`);
  }
}

function restoreBrowserSettings(values) {
  Object.entries(values || {}).forEach(([key,value]) => {
    if (typeof value === "string") {
      localStorage.setItem(key, removeLegacyAutomationReferences(key, value));
    }
  });
}

function normalizeBackupPayload(rawBackup) {
  if (!rawBackup || typeof rawBackup !== "object" || Array.isArray(rawBackup)) return null;
  const legacy = rawBackup.format === "LayZSpaBackup";
  const legacySettings = legacy && rawBackup.mqtt && rawBackup.region &&
    typeof rawBackup.mqtt === "object" && typeof rawBackup.region === "object"
    ? {mqtt: rawBackup.mqtt, region: rawBackup.region} : null;
  const files = rawBackup.files && typeof rawBackup.files === "object"
    ? rawBackup.files
    : {
        settings: rawBackup.settings ?? legacySettings,
        hardware: rawBackup.hardware ?? rawBackup.bestway_hwcfg ?? null,
        maintenance: rawBackup.maintenance ?? null
      };
  const recognized = rawBackup.format === "SpaControlBackup" ||
    rawBackup.format === "Spa Control Backup" ||
    legacySettings || files.settings || files.hardware || files.maintenance ||
    Array.isArray(rawBackup.schedules) || rawBackup.energy || rawBackup.browser;
  if (!recognized) return null;
  return {
    format: "SpaControlBackup",
    legacy: !!legacySettings,
    createdAt: rawBackup.createdAt || null,
    version: String(rawBackup.version || rawBackup.backupVersion || "1.0"),
    files,
    schedules: Array.isArray(rawBackup.schedules) ? rawBackup.schedules : null,
    energy: rawBackup.energy && typeof rawBackup.energy === "object" ? rawBackup.energy : null,
    browser: rawBackup.browser && typeof rawBackup.browser === "object" ? rawBackup.browser : null
  };
}

function validBackupSchedules(items) {
  return items.length <= 24 && items.every(item =>
    item && Number.isInteger(item.daysMask) && item.daysMask >= 1 && item.daysMask <= 127 &&
    Number.isInteger(item.hour) && item.hour >= 0 && item.hour <= 23 &&
    Number.isInteger(item.minute) && item.minute >= 0 && item.minute <= 59 &&
    Number.isInteger(item.action) && item.action >= 0 && item.action <= 8 &&
    (item.action !== 8 || (Number.isInteger(item.value) && item.value >= 20 && item.value <= 40))
  );
}

async function restoreSchedules(items, existing) {
  // Na validatie en eventuele waarschuwing alle bestaande regels vervangen.
  for (const item of existing) {
    const response = await fetch(`/api/schedules?id=${encodeURIComponent(item.id)}`, {method:"DELETE"});
    if (!response.ok) throw new Error(`Plannerregel ${item.id} verwijderen: HTTP ${response.status}`);
  }
  for (const item of items) await postForm("/api/schedules", item, "POST");
}

async function importBackup(event) {
  const file = event.target.files?.[0];
  if (!file) return;

  const results = [];
  let restoredBackendSection = false;
  const runSection = async (label, action) => {
    try {
      await action();
      results.push(`✓ ${tr(label)}`);
      restoredBackendSection = true;
      return true;
    } catch (error) {
      results.push(`✗ ${tr(label)}: ${error.message || tr("Herstellen mislukt")}`);
      return false;
    }
  };

  try {
    const text = (await file.text()).replace(/^\uFEFF/, "").trim();
    let parsed;
    try { parsed = JSON.parse(text); }
    catch (_) { throw new Error(tr("Back-upbestand kan niet worden gelezen")); }

    const backup = normalizeBackupPayload(parsed);
    if (!backup) throw new Error(tr("Ongeldig back-upbestand: structuur niet herkend"));

    if (backup.schedules !== null && !validBackupSchedules(backup.schedules)) {
      throw new Error("Ongeldige plannerregels in back-up; niets hersteld");
    }
    const message = backup.legacy
      ? `Oude back-up (${backup.createdAt || backup.version}) herstellen? Instellingen en energietellers worden vervangen. Hardware, onderhoud en browserinstellingen ontbreken in dit bestand.`
      : tr("Deze volledige back-up herstellen? Bestaande instellingen en planneritems worden vervangen.");
    if (!confirm(message)) return;

    let existingSchedules = null;
    let skipSchedules = false;
    if (backup.schedules !== null) {
      const response = await fetch("/api/schedules", {cache:"no-store"});
      if (!response.ok) throw new Error(`Planner lezen: HTTP ${response.status}; niets hersteld`);
      existingSchedules = await response.json();
      if (!Array.isArray(existingSchedules)) throw new Error("Plannerantwoord is ongeldig; niets hersteld");
      if (backup.schedules.length === 0 && existingSchedules.length > 0) {
        skipSchedules = !confirm(`Deze back-up heeft 0 plannerregels. ${existingSchedules.length} huidige plannerregels verwijderen?`);
      }
    }

    if (backup.files.settings) {
      await runSection("Instellingen hersteld", () => restoreBackupFile("settings", backup.files.settings));
    }
    if (backup.files.hardware) {
      await runSection("Hardware hersteld", () => restoreBackupFile("hardware", backup.files.hardware));
    }
    if (backup.files.maintenance) {
      await runSection("Onderhoud hersteld", () => restoreBackupFile("maintenance", backup.files.maintenance));
    }

    if (backup.schedules !== null) {
      if (skipSchedules) results.push(`• ${tr("Planner overgeslagen")}`);
      else await runSection("Planner hersteld", () => restoreSchedules(backup.schedules, existingSchedules));
    }

    if (backup.energy) {
      await runSection("Energie hersteld", () => postForm("/api/energy", backup.energy));
    }

    if (backup.browser) {
      restoreBrowserSettings(backup.browser);
      results.push(`✓ ${tr("Webinterface hersteld")}`);
    }

    if (backup.legacy) results.push("• Oude back-up: geen hardware, onderhoud of browserinstellingen aanwezig");
    results.push(`• ${tr("WiFi-instellingen behouden")}`);
    const failed = results.some(line => line.startsWith("✗"));
    // Een toast verdwijnt na enkele seconden; toon het resultaat vóór een herstart.
    alert(results.join("\n") + (failed ? "\nControleer de mislukte onderdelen vóór opnieuw herstellen." : ""));

    if (restoredBackendSection && !failed) {
      setTimeout(()=>fetch("/api/restart",{method:"POST"}),1800);
    } else if (!failed) {
      setTimeout(()=>location.reload(),800);
    }
  } catch (error) {
    showToast(error.message || tr("Herstellen mislukt"), true);
  } finally {
    event.target.value="";
  }
}


const hardwareModels = [
  "6-draads, vóór 2021 (AirJet)",
  "6-draads, 2021 (AirJet)",
  "6-draads, 2021 (AirJet + HydroJet)",
  "6-draads, 54149E (AirJet)",
  "4-draads, 54173 (AirJet + HydroJet)",
  "4-draads, 54154 (AirJet)",
  "4-draads, 54144 (HydroJet)",
  "4-draads, 54138 (AirJet + HydroJet)",
  "4-draads, 54123 (AirJet)"
];

const hardwareCapabilities = [
  { air: true,  jets: false }, // PRE2021
  { air: true,  jets: false }, // MIAMI2021
  { air: true,  jets: true  }, // MALDIVES2021
  { air: true,  jets: false }, // 54149E
  { air: true,  jets: true  }, // 54173
  { air: true,  jets: false }, // 54154
  { air: false, jets: true  }, // 54144
  { air: true,  jets: true  }, // 54138
  { air: true,  jets: false }  // 54123
];

function applyHardwareCapabilities(model) {
  const caps = hardwareCapabilities[Number(model)] || hardwareCapabilities[0];

  ["bubbles", "controlBubbles"].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.style.display = caps.air ? "" : "none";
  });

  ["jets", "controlJets"].forEach(id => {
    const el = document.getElementById(id);
    if (el) el.style.display = caps.jets ? "" : "none";
  });

  /*
   * HydroJet/Jets alleen tonen waar het geselecteerde model
   * deze functie daadwerkelijk ondersteunt.
   */
  const jetsDashboardSetting =
    document.querySelector('[data-dashboard-setting="jets"]')?.closest("label");
  if (jetsDashboardSetting) {
    jetsDashboardSetting.style.display = caps.jets ? "" : "none";
  }

  const jetsOrderItem =
    document.querySelector('[data-dashboard-button-order-item="jets"]');
  if (jetsOrderItem) {
    jetsOrderItem.style.display = caps.jets ? "" : "none";
  }

  const jetsMqttCard =
    document.getElementById("publishJets")?.closest(".mqtt-publication-card");
  if (jetsMqttCard) {
    jetsMqttCard.style.display = caps.jets ? "" : "none";
  }
}

function initHardwareSelectors() {
  ["hwCio", "hwDsp"].forEach(id => {
    const select = document.getElementById(id);
    if (!select || select.options.length) return;
    hardwareModels.forEach((name, value) => select.add(new Option(name, value)));
  });
}


function hardwareProtocolFamily(model) {
  model = Number(model);

  if (model >= 4) return "4wire";
  if (model === 3) return "type2";
  return "type1";
}

function updateHardwareLabels() {
  const cio = Number(document.getElementById("hwCio")?.value ?? 0);
  const family = hardwareProtocolFamily(cio);

  let labels;

  if (family === "4wire") {
    labels = [
      "CIO RX",
      "CIO TX",
      "Niet gebruikt",
      "DSP TX",
      "DSP RX",
      "Niet gebruikt",
      "Niet gebruikt",
      "Temperatuursensor"
    ];
  } else if (family === "type2") {
    labels = [
      "CIO TD",
      "CIO clock",
      "CIO LD",
      "DSP TD",
      "DSP clock",
      "DSP LD",
      "DSP audio",
      "Temperatuursensor"
    ];
  } else {
    labels = [
      "CIO data",
      "CIO clock",
      "CIO CS",
      "DSP data",
      "DSP clock",
      "DSP CS",
      "DSP audio",
      "Temperatuursensor"
    ];
  }

  labels.forEach((text, index) => {
    const label = document.getElementById(`hwLabel${index + 1}`);
    if (label) label.textContent = text;
  });

  const unused = family === "4wire" ? [3, 6, 7] : [];

  for (let i = 1; i <= 8; i++) {
    const input = document.getElementById(`hwPin${i}`);
    if (!input) continue;

    const disabled = unused.includes(i);
    input.disabled = disabled;

    if (disabled) input.value = 0;
  }
}

function hardwarePinPreset(cio, dsp) {
  const family = hardwareProtocolFamily(cio);
  if (family === "4wire") return [23,22,0,20,19,0,0,0];
  return [23,22,21,20,19,18,11,0];
}

function applyHardwarePins() {
  updateHardwareLabels();
  const cio = Number(document.getElementById("hwCio")?.value || 0);
  const dsp = Number(document.getElementById("hwDsp")?.value || 0);
  const pins = hardwarePinPreset(cio, dsp);
  pins.forEach((pin, index) => {
    const el = document.getElementById(`hwPin${index+1}`);
    if (el) el.value = pin;
  });
}

async function loadHardwareSettings() {
  initHardwareSelectors();
  try {
    const response = await fetch("/api/hardware", {cache:"no-store"});
    if (!response.ok) throw new Error("Hardwareconfiguratie niet beschikbaar");
    const cfg = await response.json();
    document.getElementById("hwCio").value = cfg.cio ?? 0;
    document.getElementById("hwDsp").value = cfg.dsp ?? 0;
    applyHardwareCapabilities(cfg.cio ?? 0);
    updateHardwareLabels();
    document.getElementById("hwTempSensor").checked = String(cfg.hasTempSensor) === "1";
    (cfg.pins || []).forEach((pin,index) => { const el=document.getElementById(`hwPin${index+1}`); if(el) el.value=pin; });
  } catch (error) { showToast(error.message, "error"); }
}


function updateHardwareTestButton() {
  const confirmed = document.getElementById("hwTestConfirm")?.checked === true;
  const button = document.getElementById("hwTestStart");
  if (button) button.disabled = !confirmed;
}

function hardwareTestName(name) {
  if (name === "data") return tr("Data");
  if (name === "clock") return tr("Clock");
  return tr("Select / Latch");
}

function renderHardwareTestResults(result) {
  const list = document.getElementById("hwTestResults");
  const summary = document.getElementById("hwTestSummary");
  if (!list || !summary) return;

  const pairs = Array.isArray(result.pairs) ? result.pairs : [];
  list.innerHTML = pairs.map(pair => {
    const applicable = pair.applicable !== false;
    const pass = applicable && pair.pass === true;
    const cls = applicable ? (pass ? "pass" : "fail") : "na";
    const status = applicable ? (pass ? tr("GOED") : tr("FOUT")) : tr("N.v.t.");
    const detail = applicable
      ? `${escapeHtml(pair.cioPin || "--")} ↔ ${escapeHtml(pair.dspPin || "--")} · ${tr("heen")}: ${Number(pair.forwardErrors||0)} · ${tr("terug")}: ${Number(pair.reverseErrors||0)}`
      : tr("Niet van toepassing op deze hardwareconfiguratie");
    return `<article class="hwtest-result ${cls}"><span class="hwtest-status-dot ${cls}"></span><div><strong>${escapeHtml(hardwareTestName(pair.name))}</strong><small>${detail}</small></div><b>${status}</b></article>`;
  }).join("");

  const overallPass = result.overallPass === true;
  summary.className = `hwtest-summary ${overallPass ? "pass" : "fail"}`;
  summary.innerHTML = `<span class="hwtest-status-dot ${overallPass ? "pass" : "fail"}"></span><div><strong>${overallPass ? tr("Connectortest geslaagd") : tr("Connectortest heeft fouten gevonden")}</strong><small>${overallPass ? tr("Alle geteste signaalparen werken in beide richtingen.") : tr("Controleer de rode verbindingen, connectoren, solderingen en level-shifter.")}</small></div>`;
}

async function runHardwareConnectorTest() {
  const confirmed = document.getElementById("hwTestConfirm")?.checked === true;
  if (!confirmed) { showToast(tr("Bevestig eerst dat de module los van de pomp is."), true); return; }
  const button = document.getElementById("hwTestStart");
  const summary = document.getElementById("hwTestSummary");
  if (button) { button.disabled = true; button.textContent = tr("Test bezig…"); }
  if (summary) summary.innerHTML = `<span class="hwtest-status-dot testing"></span><div><strong>${tr("Connectortest bezig…")}</strong><small>${tr("De signaallijnen worden in beide richtingen getest.")}</small></div>`;
  try {
    const response = await fetch("/api/hardware/test", {method:"POST", cache:"no-store"});
    const result = await response.json().catch(()=>({}));
    if (response.status === 409 && result.error === "spaConnected") throw new Error(tr("Test geblokkeerd: er is nog actieve communicatie met de spa. Trek eerst de netstekker uit."));
    if (!response.ok || !result.ok) throw new Error(tr("Connectortest mislukt"));
    renderHardwareTestResults(result);
    showToast(result.overallPass ? tr("Connectortest geslaagd") : tr("Connectortest heeft fouten gevonden"), !result.overallPass);
  } catch (error) {
    if (summary) summary.innerHTML = `<span class="hwtest-status-dot fail"></span><div><strong>${tr("Test niet uitgevoerd")}</strong><small>${escapeHtml(error.message || tr("Connectortest mislukt"))}</small></div>`;
    showToast(error.message || tr("Connectortest mislukt"), true);
  } finally {
    if (button) { button.textContent = tr("Start volledige connectortest"); updateHardwareTestButton(); }
  }
}

async function saveHardwareSettings() {
  const value = id => Number(document.getElementById(id).value || 0);
  const cfg = {
    cio: value("hwCio"), dsp: value("hwDsp"),
    hasTempSensor: document.getElementById("hwTempSensor").checked ? "1" : "0",
    pins: Array.from({length:8},(_,i)=>value(`hwPin${i+1}`))
  };
  try {
    const response = await fetch("/api/hardware", {method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify(cfg)});
    const result = await response.json();
    if (!response.ok || !result.ok) throw new Error(result.error || "Opslaan mislukt");
    applyHardwareCapabilities(cfg.cio);
    showToast(tr("Hardware opgeslagen. ESP wordt opnieuw opgestart..."), "success");

    /*
     * Niet wachten op het HTTP-antwoord van /api/restart:
     * de ESP verbreekt tijdens de reboot bewust de verbinding.
     * Anders ziet de browser een succesvolle reboot als een fout.
     */
    setTimeout(() => {
      fetch("/api/restart", { method: "POST" }).catch(() => {});
    }, 500);

    setTimeout(() => {
      location.reload();
    }, 8000);

  } catch(error) {
    showToast(error.message, "error");
  }
}


document.addEventListener("DOMContentLoaded", () => {
  loadDashboardDetails();
  loadDashboardButtons();
  applyDashboardSections();
  applyNavigationSettings();
  loadNavigationOrderEditor();
  loadHardwareSettings();
});




/* v3.2.0 RC7 - robuuste lokale weerinstellingen en weerkaart */
const WEATHER_SETTINGS_KEY = "layzspaWeatherSettings";
const WEATHER_CACHE_KEY = "layzspaWeatherCache";
let weatherRefreshTimer = null;

function getWeatherSettings(){
  try{
    return {enabled:true,city:"",country:"NL",interval:30,latitude:null,longitude:null,...JSON.parse(localStorage.getItem(WEATHER_SETTINGS_KEY)||"{}")};
  }catch(_){
    return {enabled:true,city:"",country:"NL",interval:30,latitude:null,longitude:null};
  }
}
function loadWeatherSettings(){
  const cfg=getWeatherSettings();
  const set=(id,value)=>{const el=document.getElementById(id);if(el)el.value=value??"";};
  set("weatherCity",cfg.city); set("weatherCountry",cfg.country||"NL"); set("weatherInterval",String(cfg.interval||30));
  const enabled=document.getElementById("weatherEnabled"); if(enabled) enabled.checked=cfg.enabled!==false;
  const result=document.getElementById("weatherLocationResult");
  if(result) result.textContent=cfg.latitude!=null&&cfg.longitude!=null?`${Number(cfg.latitude).toFixed(4)}, ${Number(cfg.longitude).toFixed(4)}`:"--";
}
async function saveWeatherSettings(){
  const previous=getWeatherSettings();
  const cfg={
    ...previous,
    enabled:document.getElementById("weatherEnabled")?.checked!==false,
    city:document.getElementById("weatherCity")?.value.trim()||"",
    country:(document.getElementById("weatherCountry")?.value.trim()||"NL").toUpperCase(),
    interval:Math.max(15,Number(document.getElementById("weatherInterval")?.value||30))
  };
  localStorage.setItem(WEATHER_SETTINGS_KEY,JSON.stringify(cfg));
  showToast(tr("Weerinstellingen opgeslagen"));
  scheduleWeatherRefresh();
  await fetchWeather(true);
  return true;
}
async function searchWeatherLocation(){
  const city=document.getElementById("weatherCity")?.value.trim()||"";
  const country=(document.getElementById("weatherCountry")?.value.trim()||"").toUpperCase();
  const result=document.getElementById("weatherLocationResult");
  if(!city){showToast(tr("Vul eerst een plaats in"),true);return;}
  if(result)result.textContent=tr("Zoeken...");
  try{
    const url=`https://geocoding-api.open-meteo.com/v1/search?name=${encodeURIComponent(city)}&count=10&language=${encodeURIComponent(activeLanguage||"nl")}&format=json`;
    const response=await fetch(url,{cache:"no-store"});
    if(!response.ok)throw new Error(tr("Locatie zoeken mislukt"));
    const data=await response.json();
    const candidates=Array.isArray(data.results)?data.results:[];
    const match=candidates.find(x=>!country||String(x.country_code||"").toUpperCase()===country)||candidates[0];
    if(!match)throw new Error(tr("Locatie niet gevonden"));
    const cfg={...getWeatherSettings(),city:match.name||city,country:String(match.country_code||country||"NL").toUpperCase(),latitude:match.latitude,longitude:match.longitude};
    localStorage.setItem(WEATHER_SETTINGS_KEY,JSON.stringify(cfg));
    loadWeatherSettings();
    if(result)result.textContent=`${cfg.city}, ${cfg.country} · ${Number(cfg.latitude).toFixed(4)}, ${Number(cfg.longitude).toFixed(4)}`;
    showToast(tr("Locatie gevonden"));
    await fetchWeather(true);
  }catch(error){if(result)result.textContent="--";showToast(error.message||tr("Locatie zoeken mislukt"),true);}
}
function weatherCodeInfo(code){
  const n=Number(code);
  if(n===0)return ["☀️",tr("Helder")];
  if([1,2].includes(n))return ["🌤️",tr("Licht bewolkt")];
  if(n===3)return ["☁️",tr("Bewolkt")];
  if([45,48].includes(n))return ["🌫️",tr("Mist")];
  if([51,53,55,56,57].includes(n))return ["🌦️",tr("Motregen")];
  if([61,63,65,66,67,80,81,82].includes(n))return ["🌧️",tr("Regen")];
  if([71,73,75,77,85,86].includes(n))return ["🌨️",tr("Sneeuw")];
  if([95,96,99].includes(n))return ["⛈️",tr("Onweer")];
  return ["🌤️",tr("Onbekend")];
}
function renderWeatherCard(record,stale=false){
  if(!record)return;
  const cfg=getWeatherSettings();
  document.getElementById("weatherDashboardCard")?.classList.toggle("dashboard-section-hidden",cfg.enabled===false);
  setText("weatherLocation",record.location||cfg.city||tr("Niet ingesteld"));
  setText("weatherTemperature",Number.isFinite(Number(record.temperature))?`${Math.round(Number(record.temperature))}°C`:"--°C");
  setText("weatherDescription",record.description||tr("Onbekend"));
  setText("weatherRain",Number.isFinite(Number(record.rain))?`${Math.round(Number(record.rain))}%`:"--%");
  setText("weatherWind",Number.isFinite(Number(record.wind))?`${Math.round(Number(record.wind))} km/u`:"-- km/u");
  setText("weatherIcon",record.icon||"🌤️");
  const stamp=record.timestamp?new Date(record.timestamp):null;
  setText("weatherUpdated",stamp&&!Number.isNaN(stamp.getTime())?`${stale?tr("Laatst bekend")+" · ":""}${stamp.toLocaleTimeString([], {hour:"2-digit",minute:"2-digit"})}`:"--:--");
}
async function fetchWeather(force=false){
  const cfg=getWeatherSettings();
  const cached=(()=>{try{return JSON.parse(localStorage.getItem(WEATHER_CACHE_KEY)||"null");}catch(_){return null;}})();
  if(cached)renderWeatherCard(cached,true);
  if(cfg.enabled===false){document.getElementById("weatherDashboardCard")?.classList.add("dashboard-section-hidden");return;}
  if(cfg.latitude==null||cfg.longitude==null){
    setText("weatherLocation",cfg.city||tr("Niet ingesteld"));
    setText("weatherDescription",tr("Locatie instellen"));
    return;
  }
  const maxAge=(Number(cfg.interval)||30)*60000;
  if(!force&&cached&&Date.now()-Number(cached.timestamp||0)<maxAge){renderWeatherCard(cached,false);return;}
  try{
    const url=`https://api.open-meteo.com/v1/forecast?latitude=${encodeURIComponent(cfg.latitude)}&longitude=${encodeURIComponent(cfg.longitude)}&current=temperature_2m,weather_code,wind_speed_10m&hourly=precipitation_probability&forecast_days=1&timezone=auto`;
    const response=await fetch(url,{cache:"no-store"});
    if(!response.ok)throw new Error("Weather HTTP "+response.status);
    const data=await response.json();
    const [icon,description]=weatherCodeInfo(data.current?.weather_code);
    const now=Date.now(); let rain=0;
    if(Array.isArray(data.hourly?.time)&&Array.isArray(data.hourly?.precipitation_probability)){
      let best=0,bestDiff=Infinity;
      data.hourly.time.forEach((t,i)=>{const diff=Math.abs(new Date(t).getTime()-now);if(diff<bestDiff){best=i;bestDiff=diff;}});
      rain=Number(data.hourly.precipitation_probability[best]||0);
    }
    const record={location:[cfg.city,cfg.country].filter(Boolean).join(", "),temperature:Number(data.current?.temperature_2m),description,icon,rain,wind:Number(data.current?.wind_speed_10m),timestamp:Date.now()};
    localStorage.setItem(WEATHER_CACHE_KEY,JSON.stringify(record)); renderWeatherCard(record,false);
  }catch(error){console.warn("Weather update failed",error);if(!cached)showToast(tr("Weer kon niet worden bijgewerkt"),true);}
}
function scheduleWeatherRefresh(){
  if(weatherRefreshTimer)clearInterval(weatherRefreshTimer);
  const minutes=Math.max(15,Number(getWeatherSettings().interval||30));
  weatherRefreshTimer=setInterval(()=>{
    if (document.getElementById("dashboardView")?.classList.contains("active")) {
      fetchWeather(true);
    }
  },minutes*60000);
}

/* ===== Internationalization v4: deterministic =====
 *
 * Eén vertaalpad voor de hele webinterface:
 * - /lang/languages.json bepaalt welke talen beschikbaar zijn.
 * - tr(key) vertaalt dynamische JavaScript-tekst.
 * - data-i18n en data-i18n-* vertalen expliciete HTML-tekst/attributen.
 * - bestaande statische HTML zonder key gebruikt éénmalig de oorspronkelijke
 *   Nederlandse tekst als legacy-key. Die brontekst wordt bewaard en verandert
 *   nooit wanneer van taal wordt gewisseld.
 * - dynamisch toegevoegde DOM wordt NIET meer op losse tekstnodes 'geraden'.
 *   Dynamische tekst hoort via tr() te worden gemaakt.
 * - taalwissels zijn race-safe: alleen de laatst gekozen taal mag toepassen.
 */
let activeLanguage = "nl";
let languagePack = {};
let dutchLanguagePack = {};
let supportedLanguages = {
  nl: "Nederlands",
  en: "English",
  de: "Deutsch",
  fr: "Français"
};
let languageRequestId = 0;
let deferredInstallPrompt = null;

const originalTextNodes = new Map();
const originalAttributes = new WeakMap();
const missingTranslationKeys = new Set();

function browserLanguage() {
  const lang = (navigator.language || "nl").slice(0, 2).toLowerCase();
  return supportedLanguages[lang] ? lang : "en";
}

async function loadLanguageCatalog() {
  try {
    const response = await fetch("/lang/languages.json", {cache:"no-store"});
    if (!response.ok) throw new Error(`Language catalog ${response.status}`);
    const catalog = await response.json();
    if (catalog && typeof catalog === "object" && Object.keys(catalog).length) {
      supportedLanguages = catalog;
    }
  } catch (error) {
    console.warn("[i18n] languages.json niet beschikbaar; standaardlijst actief", error);
  }
  updateLanguageSelector();
}

function updateLanguageSelector() {
  const select = document.getElementById("languageSelect");
  if (!select) return;
  const wanted = localStorage.getItem("layzspaLanguage") || activeLanguage || "nl";
  select.replaceChildren();
  Object.entries(supportedLanguages).forEach(([code,label]) => {
    const option = document.createElement("option");
    option.value = code;
    option.textContent = label;
    select.appendChild(option);
  });
  select.value = supportedLanguages[wanted] ? wanted : "nl";
}

async function loadLanguageFile(lang) {
  const response = await fetch(`/lang/${lang}.json?v=5001`, {cache:"no-store"});
  if (!response.ok) throw new Error(`Language file ${lang}: ${response.status}`);
  return await response.json();
}

function packValue(pack, key) {
  return pack?.[key] ?? pack?._phrases?.[key];
}

function tr(source) {
  if (source === null || source === undefined) return "";
  const key = String(source);
  const activeValue = packValue(languagePack, key);
  if (activeValue !== undefined) return activeValue;

  const fallbackValue = packValue(dutchLanguagePack, key);
  if (activeLanguage !== "nl") {
    const warningKey = `${activeLanguage}:${key}`;
    if (!missingTranslationKeys.has(warningKey)) {
      missingTranslationKeys.add(warningKey);
      console.warn(`[i18n] ontbrekende vertaling (${activeLanguage}): ${key}`);
    }
  }
  return fallbackValue ?? key;
}

function rememberTextNode(node) {
  if (!originalTextNodes.has(node)) originalTextNodes.set(node, node.nodeValue);
}

function translateTextNode(node) {
  rememberTextNode(node);
  const raw = originalTextNodes.get(node) ?? "";
  const trimmed = raw.trim();
  if (!trimmed) return;
  node.nodeValue = raw.replace(trimmed, tr(trimmed));
}

function translateDom(root = document.body) {
  if (!root) return;

  /* Expliciete keys eerst. */
  if (root.nodeType === Node.ELEMENT_NODE && root.hasAttribute?.("data-i18n")) {
    const key = root.getAttribute("data-i18n");
    if (key) root.textContent = tr(key);
  }
  root.querySelectorAll?.("[data-i18n]").forEach(el => {
    const key = el.getAttribute("data-i18n");
    if (key) el.textContent = tr(key);
  });

  /* Legacy statische HTML: brontekst exact één keer bewaren. */
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
    acceptNode(node) {
      if (!node.parentElement) return NodeFilter.FILTER_REJECT;
      if (["SCRIPT","STYLE"].includes(node.parentElement.tagName)) return NodeFilter.FILTER_REJECT;
      if (node.parentElement.hasAttribute("data-i18n")) return NodeFilter.FILTER_REJECT;
      return node.nodeValue.trim() ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT;
    }
  });
  const nodes=[];
  while (walker.nextNode()) nodes.push(walker.currentNode);
  nodes.forEach(translateTextNode);

  /* Attributen: expliciete key heeft voorrang; anders originele tekst bewaren. */
  const attrSelector = "[title],[aria-label],[placeholder]";
  const elements=[];
  if (root.nodeType === Node.ELEMENT_NODE && root.matches?.(attrSelector)) elements.push(root);
  root.querySelectorAll?.(attrSelector).forEach(el => elements.push(el));
  elements.forEach(el => {
    let saved = originalAttributes.get(el);
    if (!saved) { saved={}; originalAttributes.set(el,saved); }
    ["title","aria-label","placeholder"].forEach(attr => {
      if (!el.hasAttribute(attr)) return;
      const explicitKey = el.getAttribute(`data-i18n-${attr}`);
      if (explicitKey) {
        el.setAttribute(attr, tr(explicitKey));
        return;
      }
      if (!(attr in saved)) saved[attr] = el.getAttribute(attr);
      el.setAttribute(attr, tr(saved[attr]));
    });
  });
}

async function applyLanguage(lang) {
  const requestId = ++languageRequestId;
  const chosen = supportedLanguages[lang] ? lang : "nl";
  try {
    if (!Object.keys(dutchLanguagePack).length) dutchLanguagePack = await loadLanguageFile("nl");
    const nextPack = chosen === "nl" ? dutchLanguagePack : await loadLanguageFile(chosen);
    if (requestId !== languageRequestId) return; // gebruiker koos inmiddels een andere taal
    activeLanguage = chosen;
    languagePack = nextPack;
    document.documentElement.lang = activeLanguage;
  } catch (error) {
    if (requestId !== languageRequestId) return;
    console.error("[i18n] taalbestand laden mislukt", error);
    activeLanguage = "nl";
    languagePack = dutchLanguagePack || {};
    document.documentElement.lang = "nl";
  }

  const select = document.getElementById("languageSelect");
  if (select) select.value = activeLanguage;

  translateDom(document.body);
  updateLocalizedFileInputs?.();
  loadSchedules?.();
  render?.();
  updateSmartInsight?.();
  translateDom(document.body);
  fetchWeather?.(false);
}

async function selectLanguage(lang) {
  const chosen = supportedLanguages[lang] ? lang : "nl";
  localStorage.setItem("layzspaLanguageMode","manual");
  localStorage.setItem("layzspaLanguage",chosen);
  const mode=document.getElementById("languageMode");
  const select=document.getElementById("languageSelect");
  if (mode) mode.value="manual";
  if (select) select.value=chosen;
  document.getElementById("manualLanguageField")?.classList.remove("hidden");
  await applyLanguage(chosen);
}

async function languageModeChanged() {
  const mode=document.getElementById("languageMode")?.value || "manual";
  localStorage.setItem("layzspaLanguageMode",mode);
  document.getElementById("manualLanguageField")?.classList.toggle("hidden",mode==="browser");
  if (mode === "browser") await applyLanguage(browserLanguage());
  else await selectLanguage(document.getElementById("languageSelect")?.value || localStorage.getItem("layzspaLanguage") || "nl");
}

async function saveLanguageSettings() {
  await selectLanguage(document.getElementById("languageSelect")?.value || "nl");
}

async function loadLanguageSettings() {
  await loadLanguageCatalog();
  const mode=localStorage.getItem("layzspaLanguageMode") || "manual";
  const saved=localStorage.getItem("layzspaLanguage") || "nl";
  const lang=supportedLanguages[saved] ? saved : "nl";
  const m=document.getElementById("languageMode"), sel=document.getElementById("languageSelect");
  if (m) m.value=mode;
  if (sel) sel.value=lang;
  document.getElementById("manualLanguageField")?.classList.toggle("hidden",mode==="browser");
  await applyLanguage(mode === "browser" ? browserLanguage() : lang);
}
window.addEventListener("beforeinstallprompt",e=>{e.preventDefault();deferredInstallPrompt=e;document.getElementById("installAppButton")?.classList.add("available");});
async function installPwa(){if(deferredInstallPrompt){deferredInstallPrompt.prompt();await deferredInstallPrompt.userChoice;deferredInstallPrompt=null;}else showToast(activeLanguage==="de"?"Im Browsermenü 'App installieren' wählen":activeLanguage==="fr"?"Choisissez ‘Installer l’application’ dans le menu du navigateur":activeLanguage==="en"?"Choose ‘Install app’ in the browser menu":"Kies ‘App installeren’ in het browsermenu");}
const accents={blue:"#19a8ff",green:"#00df9a",orange:"#ff9f43",red:"#ff4d5e",purple:"#9b7bff"};
// Browser-only visual styles: no firmware settings, requests or timers.
function applyVisualStyle(value) {
  const style = ["premium", "resort", "console"].includes(value) ? value : "premium";
  document.documentElement.dataset.visualStyle = style;
  const selector = document.getElementById("visualStyle");
  if (selector) selector.value = style;
}
function loadAppearanceSettings() {
  applyVisualStyle(localStorage.getItem("layzspaVisualStyle"));
  const a = localStorage.getItem("layzspaAccent") || "blue";
  document.documentElement.style.setProperty("--blue", accents[a] || accents.blue);
  const sel = document.getElementById("accentTheme");
  if (sel) sel.value = accents[a] ? a : "blue";
  const motion = localStorage.getItem("layzspaMotion") !== "false";
  const compact = localStorage.getItem("layzspaCompact") === "true";
  document.body.classList.toggle("reduce-motion", !motion);
  document.body.classList.toggle("compact-cards", compact);
  if (document.getElementById("motionEnabled")) document.getElementById("motionEnabled").checked = motion;
  if (document.getElementById("compactCards")) document.getElementById("compactCards").checked = compact;
}
function saveAppearanceSettings() {
  const style = document.getElementById("visualStyle")?.value || "premium";
  localStorage.setItem("layzspaVisualStyle", ["premium", "resort", "console"].includes(style) ? style : "premium");
  const a = document.getElementById("accentTheme")?.value || "blue";
  localStorage.setItem("layzspaAccent", accents[a] ? a : "blue");
  localStorage.setItem("layzspaMotion", document.getElementById("motionEnabled")?.checked !== false);
  localStorage.setItem("layzspaCompact", document.getElementById("compactCards")?.checked === true);
  loadAppearanceSettings();
  showToast(tr("Weergave opgeslagen"));
}
function loadWidgetSettings(){const on=localStorage.getItem("layzspaSmartInsight")!=="false";document.getElementById("smartInsightCard")?.classList.toggle("dashboard-section-hidden",!on);if(document.getElementById("smartInsightEnabled"))document.getElementById("smartInsightEnabled").checked=on;}
function saveWidgetSettings(){localStorage.setItem("layzspaSmartInsight",document.getElementById("smartInsightEnabled")?.checked!==false);loadWidgetSettings();}
function updateSmartInsight(){const t=Number(state.temperature),g=Number(state.target),active=!!state.heaterActive;let title,text;if(active){title=tr("Water wordt verwarmd");text=tr("De heater levert nu actief warmte.");}else if(Number.isFinite(t)&&Number.isFinite(g)&&g>0&&t>=g){title=tr("Spa op temperatuur");text=tr("De gewenste watertemperatuur is bereikt.");}else if(Number.isFinite(t)&&Number.isFinite(g)&&g>t){title=tr("Water onder doeltemperatuur");text=tr("Schakel de heater in om het water op temperatuur te brengen.");}else{title=tr("Spa gereed");text=tr("Live gegevens worden geanalyseerd.");}setText("smartInsightTitle",title);setText("smartInsightText",text);}
// Dynamic values must use the same translation engine.
const baseRender = render;
render=function(){baseRender();
  const hb=document.getElementById("heater")?.querySelector("small"); if(hb)hb.innerText=state.heaterActive?tr("Verwarmt"):(state.heater?tr("Stand-by"):tr("Uit"));
  document.getElementById("heater")?.classList.toggle("heating-now", !!state.heaterActive);
  setControlActive("controlLock",state.locked,state.locked?tr("Vergrendeld"):tr("Vrij"));
  setControlActive("controlTimer",state.timerActive,state.timerActive?tr("Actief"):tr("Uit"));
  setControlActive("controlHeater",state.heater,state.heaterActive?tr("Verwarmt"):(state.heater?tr("Stand-by"):tr("Uit")));
  setDot("wifiStatus",!!state.wifi,tr("Verbonden"),tr("Offline")); setDot("mqttStatus",!!state.mqtt,tr("Online"),tr("Offline"));
  updateSmartInsight();
};
const baseShowView=showView;
showView=function(viewName){baseShowView(viewName);const titles={dashboard:"Bestway Lay-Z-Spa",planner:"Planner",history:"Historie",energy:"Energie",logs:"Diagnostiek",settings:"Instellingen",hardware:"Hardware",hardwareTest:"Connectortest",control:"Bedieningspaneel",personalization:"Interface aanpassen",maintenance:"Onderhoud",info:"Informatie"};setText("pageTitle",tr(titles[viewName]||"Bestway Lay-Z-Spa"));setTimeout(()=>translateDom(document.querySelector(`#${viewName}View`)||document.body),0);};
function updateLocalizedFileInputs(){
  document.querySelectorAll(".localized-file-input").forEach(wrapper=>{
    const input=wrapper.querySelector('input[type="file"]');
    const button=wrapper.querySelector(".file-select-button");
    const name=wrapper.querySelector(".file-selected-name");
    if(button) button.textContent=tr("Bestand kiezen");
    if(name) name.textContent=input?.files?.[0]?.name || tr("Geen bestand gekozen");
  });
}

function initializeLocalizedFileInputs(){
  document.querySelectorAll(".localized-file-input input[type=file]").forEach(input=>{
    if(input.dataset.localizedReady==="1") return;
    input.dataset.localizedReady="1";
    input.addEventListener("change",updateLocalizedFileInputs);
  });
  updateLocalizedFileInputs();
}


function initSettingsAccordions() {
  document.querySelectorAll("#settingsView > .settings-card, #personalizationView > .settings-card").forEach((card, index) => {
    if (card.dataset.accordionReady === "1") return;
    const title = card.querySelector(":scope > .settings-title");
    if (!title) return;
    card.dataset.accordionReady = "1";
    card.classList.add("settings-collapsible", "settings-collapsed");
    title.setAttribute("role", "button");
    title.setAttribute("tabindex", "0");
    title.setAttribute("aria-expanded", "false");
    const toggle = () => {
      const collapsed = card.classList.toggle("settings-collapsed");
      title.setAttribute("aria-expanded", collapsed ? "false" : "true");
    };
    title.addEventListener("click", event => {
      if (event.target.closest("button,input,select,label,a")) return;
      toggle();
    });
    title.addEventListener("keydown", event => {
      if (event.key === "Enter" || event.key === " ") { event.preventDefault(); toggle(); }
    });
  });
  const mqttPanel = document.getElementById("mqttSettingsPanel");
  const mqttToggle = document.getElementById("mqttSettingsToggle");
  if (mqttPanel) mqttPanel.hidden = true;
  if (mqttToggle) { mqttToggle.classList.remove("open"); mqttToggle.setAttribute("aria-expanded","false"); }
}

const observer=new MutationObserver(mutations=>{for(const m of mutations){m.addedNodes.forEach(n=>{if(n.nodeType===Node.ELEMENT_NODE)translateDom(n);});}});
document.addEventListener("DOMContentLoaded",()=>{resetLiveControls();initSettingsAccordions();loadLanguageSettings();loadAppearanceSettings();loadWidgetSettings();applyDashboardOrder();loadDashboardOrderEditor();applyDashboardButtonOrder();loadDashboardButtonOrderEditor();initializeLocalizedFileInputs();loadWeatherSettings();loadMaintenance();fetchWeather();scheduleWeatherRefresh();const unitSelect=document.getElementById("temperatureUnit");if(unitSelect){unitSelect.addEventListener("change",()=>{preferredTemperatureUnit=unitSelect.value||"Celsius";localStorage.setItem("layzspaTemperatureUnit",preferredTemperatureUnit);applyTemperatureUnit();});}observer.observe(document.body,{childList:true,subtree:true});if("serviceWorker" in navigator)navigator.serviceWorker.register("/sw.js?v=3256").catch(()=>{});});


let matterWindowTimer = null;
let matterWindowDeadline = 0;
function renderMatterQr(id, payload) {
  const image = document.getElementById(id);
  if (!image) return;
  if (!payload) { image.hidden = true; image.removeAttribute('src'); delete image.dataset.payload; return; }
  if (image.dataset.payload === payload) return;
  try {
    const qr = qrcode(0, 'M');
    qr.addData(payload, 'Alphanumeric'); qr.make();
    image.src = qr.createDataURL(6, 24);
    image.dataset.payload = payload; image.hidden = false;
  } catch (_) { image.hidden = true; showToast(tr('QR-code maken mislukt'), true); }
}
function updateMatterCountdown() {
  const remaining = Math.max(0, Math.ceil((matterWindowDeadline - Date.now()) / 1000));
  setText('matterWindowRemaining', remaining ? `Venster maximaal nog ${Math.floor(remaining / 60)}:${String(remaining % 60).padStart(2,'0')}` : 'Koppelvenster verlopen');
  if (!remaining) {
    clearInterval(matterWindowTimer); matterWindowTimer = null;
    const panel = document.getElementById('matterTemporaryPairing');
    if (panel) panel.hidden = true;
    renderMatterQr('matterTemporaryQrImage', '');
    setText('matterTemporaryCode', '--'); setText('matterTemporaryQrPayload', '--');
    if (document.getElementById('settingsView')?.classList.contains('active')) loadMatterSettings();
  }
}
function applyMatterSettings(d) {
  const e = document.getElementById('matterEnabled'); if (e) e.checked = !!d.enabled;
  setText('matterModuleId', d.moduleId || '--');
  setText('matterRuntime', d.runtimeActive ? 'Actief' : 'Inactief');
  setText('matterFabricCount', d.fabricCount ?? 0);
  setText('matterStatus', d.credentialsReady === false ? 'Credentials niet beschikbaar' : !d.enabled ? (d.runtimeActive ? 'Uitschakelen... herstart nodig' : 'Uitgeschakeld') : !d.runtimeActive ? 'Ingeschakeld, herstart nodig' : d.commissioned ? 'Gekoppeld' : 'Klaar voor koppelen');
  const list = document.getElementById('matterFabrics');
  if (list) {
    list.replaceChildren();
    for (const fabric of (d.fabrics || [])) {
      const row = document.createElement('div');
      row.textContent = `Fabric ${fabric.index}: ${fabric.label || 'geen label'} · vendor 0x${Number(fabric.vendorId).toString(16).toUpperCase()} · ID ${fabric.fabricId}`;
      list.appendChild(row);
    }
  }
  const pairing = document.getElementById('matterPairing'); if (pairing) pairing.hidden = !d.enabled || !d.credentialsReady;
  setText('matterManualCode', d.manualPairingCode || '--'); setText('matterQrPayload', d.qrPayload || '--');
  renderMatterQr('matterQrImage', d.credentialsReady ? d.qrPayload : '');
  const button = document.getElementById('matterOpenWindow'); if (button) button.disabled = !d.runtimeActive || !d.credentialsReady || !!d.windowOpening;
  clearInterval(matterWindowTimer); matterWindowTimer = null;
  const temporary = !!d.commissioningWindowOpen && !!d.temporaryPairingCode && d.windowRemainingSeconds > 0;
  const panel = document.getElementById('matterTemporaryPairing'); if (panel) panel.hidden = !temporary;
  setText('matterTemporaryCode', temporary ? d.temporaryPairingCode : '--');
  setText('matterTemporaryQrPayload', temporary ? d.temporaryQrPayload : '--');
  renderMatterQr('matterTemporaryQrImage', temporary ? d.temporaryQrPayload : '');
  if (temporary) {
    matterWindowDeadline = Date.now() + d.windowRemainingSeconds * 1000;
    updateMatterCountdown(); matterWindowTimer = setInterval(updateMatterCountdown, 1000);
  }
}
async function loadMatterSettings() {
  try {
    const r = await fetch('/api/settings/matter', {cache:'no-store'});
    if (!r.ok) throw new Error('Matter-status ophalen mislukt');
    applyMatterSettings(await r.json());
  } catch (e) { showToast(e.message, true); }
}
let matterWindowRequestPending = false;
async function openMatterWindow() {
  if (matterWindowRequestPending) return;
  matterWindowRequestPending = true;
  const button = document.getElementById('matterOpenWindow');
  const label = button?.textContent || 'Nieuwe koppeling openen';
  if (button) { button.disabled = true; button.textContent = 'Koppelvenster openen…'; }
  try {
    const r = await fetch('/api/settings/matter', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:new URLSearchParams({action:'openWindow'}), signal:AbortSignal.timeout(7000)});
    if (!r.ok) throw new Error((await r.text()) || 'Koppelvenster aanvragen mislukt');
    const accepted = await r.json();
    if (!accepted.ok) throw new Error('Koppelvenster niet aangevraagd');
    // Bounded status checks only for this explicit action; no background polling.
    for (let attempt = 0; attempt < 10; attempt++) {
      await new Promise(resolve => setTimeout(resolve, attempt ? 700 : 300));
      const response = await fetch('/api/settings/matter', {cache:'no-store', signal:AbortSignal.timeout(7000)});
      if (!response.ok) throw new Error('Koppelstatus ophalen mislukt');
      const d = await response.json();
      if (d.windowOpening) continue;
      applyMatterSettings(d);
      if (d.windowOpenError) throw new Error(`Matter koppelvenster mislukt (fout ${d.windowOpenError}). Controleer het logboek.`);
      if (!d.commissioningWindowOpen) throw new Error('Matter heeft geen koppelvenster geopend');
      showToast(d.temporaryPairingCode ? 'Gebruik de tijdelijke code voor de extra koppeling' : 'Gebruik de vaste apparaatcode voor de eerste koppeling');
      return;
    }
    throw new Error('Openen nog niet bevestigd. Gebruik Matter-status vernieuwen.');
  } catch (e) {
    showToast(e.name === 'TimeoutError' ? 'Geen antwoord op koppelactie. Controleer de ESP-status.' : e.message, true);
  } finally {
    matterWindowRequestPending = false;
    if (button) { button.textContent = label; button.disabled = false; }
  }
}
async function copyMatterCode(id = 'matterManualCode') {
  const code = document.getElementById(id)?.textContent?.trim(); if (!code || code === '--') return;
  try { await navigator.clipboard.writeText(code); showToast(tr('Matter-koppelcode gekopieerd')); }
  catch (_) { showToast(tr('Kopiëren mislukt'),true); }
}
async function saveMatterSettings(){const enabled=!!document.getElementById('matterEnabled')?.checked;try{const r=await fetch('/api/settings/matter',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams({enabled:enabled?'true':'false'})});if(!r.ok)throw new Error(tr('Matter-instelling opslaan mislukt'));const d=await r.json();showToast(d.restartScheduled?(enabled?tr('Matter wordt ingeschakeld; controller herstart'):tr('Matter wordt uitgeschakeld; controller herstart')):tr('Matter-instelling opgeslagen'));if(d.restartScheduled)setTimeout(()=>location.reload(),5000);}catch(e){showToast(e.message,true);}}
async function resetMatterPairing(){if(!confirm(tr('Alle Matter-koppelingen wissen, een nieuwe vaste apparaatcode maken en de controller herstarten? WiFi en MQTT blijven behouden.')))return;try{const r=await fetch('/api/matter/reset',{method:'POST'});if(!r.ok)throw new Error(tr('Matter reset mislukt'));showToast(tr('Matter-koppeling gewist'));}catch(e){showToast(e.message,true);}}


async function loadDemoMode() {
  const checkbox = document.getElementById("demoModeEnabled");
  if (!checkbox) return;

  try {
    const response = await fetch(
      "/api/settings/demo",
      {cache:"no-store"}
    );

    if (!response.ok) {
      throw new Error(tr("Demo-instelling niet beschikbaar"));
    }

    const result = await response.json();
    checkbox.checked = result.enabled === true;
    updateDemoModeWarning();

  } catch (error) {
    console.warn("Demo-modus laden mislukt:", error);
  }
}


function updateDemoModeWarning() {
  const enabled =
    document.getElementById("demoModeEnabled")?.checked === true;

  const warning =
    document.getElementById("demoModeWarning");

  if (warning) {
    warning.hidden = !enabled;
  }
}


async function saveDemoMode() {
  const checkbox =
    document.getElementById("demoModeEnabled");

  if (!checkbox) return;

  const enabled = checkbox.checked;

  try {
    const response = await fetch(
      "/api/settings/demo",
      {
        method: "POST",
        headers: {
          "Content-Type": "application/json"
        },
        body: JSON.stringify({
          enabled: enabled ? 1 : 0
        })
      }
    );

    const result =
      await response.json().catch(()=>({}));

    if (!response.ok || !result.ok) {
      throw new Error(
        result.error ||
        tr("Demo-modus opslaan mislukt")
      );
    }

    showToast(
      enabled
        ? tr("Demo-modus ingeschakeld. ESP wordt opnieuw opgestart...")
        : tr("Demo-modus uitgeschakeld. ESP wordt opnieuw opgestart..."),
      "success"
    );

    /*
     * De instelling wordt bij boot door BestwayBus gelezen.
     */
    setTimeout(() => {
      fetch(
        "/api/restart",
        {method:"POST"}
      ).catch(()=>{});
    }, 500);

  } catch (error) {
    showToast(
      error.message ||
      tr("Demo-modus opslaan mislukt"),
      true
    );
  }
}


/*
 * Demo-status ophalen zodra de webinterface geladen is.
 */
window.addEventListener("load", () => {
  loadDemoMode();
});
