// feature_examples.js - 각 feature 개념을 보여주는 미니 SVG 예시(표의 '예시' 열)
"use strict";

// 44x26 viewBox, phosphor green. 개념 직관용 도식.
var G = "#46ff85", O = "#ffb642";
var FEATURE_EXAMPLE = {
  // 크기/기본
  area:        '<ellipse cx="22" cy="13" rx="13" ry="8" fill="' + G + '" opacity="0.55"/>',
  area_holes:  '<circle cx="22" cy="13" r="10" fill="' + G + '" opacity="0.55"/><circle cx="22" cy="13" r="4" fill="#000"/>',
  contlength:  '<ellipse cx="22" cy="13" rx="13" ry="8" fill="none" stroke="' + G + '" stroke-width="2" stroke-dasharray="3 2"/>',
  diameter:    '<circle cx="22" cy="13" r="9" fill="none" stroke="' + G + '" stroke-width="1.5"/><line x1="14" y1="19" x2="30" y2="7" stroke="' + O + '" stroke-width="1.5"/>',

  // 형상 계수
  circularity: '<circle cx="13" cy="13" r="8" fill="none" stroke="' + G + '" stroke-width="2"/><path d="M28,6 q10,4 2,9 q6,6 -4,5 q-8,-1 0,-7 q-4,-6 2,-7Z" fill="none" stroke="#888" stroke-width="1.5"/>',
  compactness: '<circle cx="13" cy="13" r="8" fill="none" stroke="' + G + '" stroke-width="2"/><rect x="24" y="9" width="16" height="8" fill="none" stroke="#888" stroke-width="1.5"/>',
  convexity:   '<path d="M8,7 L36,7 L36,20 L22,12 L8,20 Z" fill="' + G + '" opacity="0.55"/>',
  rectangularity: '<rect x="10" y="6" width="24" height="14" fill="none" stroke="' + G + '" stroke-width="2"/>',
  roundness:   '<circle cx="13" cy="13" r="8" fill="none" stroke="' + G + '" stroke-width="2"/><path d="M31,13 a7,7 0 1,0 0.1,0 M28,7 l1,2 M35,9 l-2,1 M35,17 l-2,-1 M28,19 l1,-2" fill="none" stroke="#888" stroke-width="1.3"/>',
  sides:       '<polygon points="22,4 34,10 31,22 13,22 10,10" fill="none" stroke="' + G + '" stroke-width="2"/>',

  // 타원/방향
  ra:          '<ellipse cx="22" cy="13" rx="14" ry="6" fill="none" stroke="' + G + '" stroke-width="1.5"/><line x1="8" y1="13" x2="36" y2="13" stroke="' + O + '" stroke-width="1.5"/>',
  rb:          '<ellipse cx="22" cy="13" rx="14" ry="6" fill="none" stroke="' + G + '" stroke-width="1.5"/><line x1="22" y1="7" x2="22" y2="19" stroke="' + O + '" stroke-width="1.5"/>',
  anisometry:  '<ellipse cx="13" cy="13" r="7" rx="7" ry="7" fill="none" stroke="#888" stroke-width="1.5"/><ellipse cx="30" cy="13" rx="12" ry="4" fill="none" stroke="' + G + '" stroke-width="2"/>',
  bulkiness:   '<ellipse cx="22" cy="13" rx="13" ry="7" fill="none" stroke="' + G + '" stroke-width="2"/>',
  structure_factor: '<ellipse cx="22" cy="13" rx="14" ry="5" fill="none" stroke="' + G + '" stroke-width="2" stroke-dasharray="3 2"/>',
  orientation: '<line x1="10" y1="20" x2="34" y2="6" stroke="' + G + '" stroke-width="2"/><path d="M10,20 a10,10 0 0,1 6,-9" fill="none" stroke="#888" stroke-width="1"/>',

  // 위상
  holes:        '<circle cx="22" cy="13" r="10" fill="' + G + '" opacity="0.55"/><circle cx="22" cy="13" r="4" fill="#000"/>',
  euler_number: '<circle cx="22" cy="13" r="10" fill="' + G + '" opacity="0.55"/><circle cx="22" cy="13" r="4" fill="#000"/>',
  connect_components: '<circle cx="13" cy="13" r="6" fill="' + G + '" opacity="0.55"/><circle cx="31" cy="13" r="6" fill="' + G + '" opacity="0.55"/>',

  // 런/두께
  thickness_mean: '<path d="M8,13 h28" stroke="' + G + '" stroke-width="6" stroke-linecap="round" opacity="0.6"/>',
  thickness_max:  '<path d="M8,13 h12" stroke="' + G + '" stroke-width="4" stroke-linecap="round" opacity="0.6"/><path d="M20,13 h16" stroke="' + G + '" stroke-width="8" stroke-linecap="round" opacity="0.6"/>',
  num_runs:    '<line x1="8" y1="7" x2="30" y2="7" stroke="' + G + '" stroke-width="2"/><line x1="8" y1="13" x2="36" y2="13" stroke="' + G + '" stroke-width="2"/><line x1="8" y1="19" x2="24" y2="19" stroke="' + G + '" stroke-width="2"/>',
  k_factor:    '<line x1="8" y1="7" x2="30" y2="7" stroke="' + G + '" stroke-width="2"/><line x1="8" y1="13" x2="36" y2="13" stroke="' + G + '" stroke-width="2"/><line x1="8" y1="19" x2="24" y2="19" stroke="' + G + '" stroke-width="2"/>'
};

// key → 미니 SVG 예시(없으면 "").
function featureExample(key) {
  var body = FEATURE_EXAMPLE[key];
  if (!body) return "";
  return '<svg class="feat-ex-svg" viewBox="0 0 44 26" width="44" height="26">' + body + "</svg>";
}
