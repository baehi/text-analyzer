#include <emscripten/emscripten.h>
#include <string>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <cstring>   // strdup

// ----------------------------------------------------
// 1. Stopwords (완전히 버릴 단어들: 조사, 연결어 등)
// ----------------------------------------------------
static const std::unordered_set<std::string> STOPWORDS = {
    // 단독 조사
    "은", "는", "이", "가",
    "을", "를",
    "에", "에서", "에게", "으로", "으로써", "부터", "까지",
    "와", "과",
    "도", "만",
    // 연결어/불용어 느낌
    "및", "등",
    "때문에", "위해", "통해"
};

// ----------------------------------------------------
// 2. 도우미 함수들
// ----------------------------------------------------

// (1) 앞뒤에 붙은 ASCII 구두점 제거: .,!?;:"'()[]{}<>
std::string trim_punct(const std::string& w) {
    if (w.empty()) return w;

    const std::string punct = ".,!?;:\"'()[]{}<>";

    size_t start = 0;
    while (start < w.size() && punct.find(w[start]) != std::string::npos) {
        start++;
    }

    size_t end = w.size();
    while (end > start && punct.find(w[end - 1]) != std::string::npos) {
        end--;
    }

    return w.substr(start, end - start);
}

// (2) ASCII 기준 정규화
//     - 영문/숫자: 남기고, 영문은 소문자로
//     - 그 외(한글 등)는 그대로 보존
std::string normalize_word(const std::string& w) {
    std::string trimmed = trim_punct(w);
    std::string res;
    res.reserve(trimmed.size());

    for (unsigned char uc : trimmed) {
        if (uc < 128) {
            // ASCII 영역
            if (std::isalnum(uc)) {
                res.push_back(static_cast<char>(std::tolower(uc)));
            } else {
                // 기타 특수문자는 버림
                continue;
            }
        } else {
            // 비-ASCII (대부분 한글) 은 그대로 보존
            res.push_back(static_cast<char>(uc));
        }
    }

    return res;
}

// (3) 문자열이 특정 접미사로 끝나는지 확인
bool ends_with(const std::string& s, const std::string& suffix) {
    if (s.size() < suffix.size()) return false;
    return std::equal(suffix.rbegin(), suffix.rend(), s.rbegin());
}

// (4) 명사 + 조사 형태에서 뒤 조사 떼어내기
//     예: "국회의" -> "국회", "과반수의" -> "과반수", "때에는" -> "때"
std::string strip_josa(const std::string& w) {
    static const std::vector<std::string> JO_ENDINGS = {
        "에는", "에게는",
        "으로써", "으로서",
        "으로는", "으로",
        "까지", "부터",
        "에서", "에게",
        "에는",
        "에",
        "의",
        "은", "는", "이", "가",
        "을", "를",
        "와", "과",
        "도", "만"
    };

    for (const auto& suf : JO_ENDINGS) {
        // 너무 짧은 단어에서까지 떼면 아무 것도 안 남을 수 있으니
        if (w.size() > suf.size() * 2 && ends_with(w, suf)) {
            return w.substr(0, w.size() - suf.size());
        }
    }
    return w;
}

// (5) "노이즈 단어" 판별: 동사·형용사·너무 짧은 것 등
bool is_noise_word(const std::string& w) {
    // 한글 1글자 ≒ 3바이트 → 3바이트 이하면 너무 짧다고 보고 버림
    if (w.size() <= 3) return true;

    // stopword 목록에 있는 것
    if (STOPWORDS.find(w) != STOPWORDS.end()) return true;

    // 동사/형용사 느낌의 끝말
    static const std::vector<std::string> VERB_ENDINGS = {
        "한다", "된다", "있다", "가진다", "받는다",
        "하였다", "하며", "하면서",
        "위하여", "의하여"
    };
    for (const auto& suf : VERB_ENDINGS) {
        if (ends_with(w, suf)) return true;
    }

    // 형용사스러운 것들
    static const std::vector<std::string> ADJ_ENDINGS = {
        "관한", "관련한"
    };
    for (const auto& suf : ADJ_ENDINGS) {
        if (ends_with(w, suf)) return true;
    }

    return false;
}

// ----------------------------------------------------
// 3. WebAssembly에서 호출되는 C++ 함수
//    - JS에서 문자열을 받아서 분석하고
//    - 결과를 하나의 문자열로 만들어서 반환
// ----------------------------------------------------

extern "C" {

// JS에서 호출할 함수
//  JS에서는 cwrap으로 "string" 반환 타입으로 사용:
//    const result = processText(text);
EMSCRIPTEN_KEEPALIVE
char* process_text(const char* raw) {
    if (!raw) return strdup("입력 오류\n");

    std::string text(raw);
    std::unordered_map<std::string, int> freq;
    std::istringstream iss(text);
    std::string word;

    // 단어 단위로 읽어와서 정규화 + 조사 제거 + 노이즈 제거 후 카운트
    while (iss >> word) {
        // 1) 기본 정규화
        std::string norm = normalize_word(word);
        if (norm.empty()) continue;

        // 2) 뒤에 붙은 조사 제거
        norm = strip_josa(norm);
        if (norm.empty()) continue;

        // 3) stopword / 동사 / 너무 짧은 것 제거
        if (is_noise_word(norm)) continue;

        // 4) 카운트 증가
        freq[norm]++;
    }

    // map -> vector 로 옮겨서, 많이 나온 순으로 정렬
    std::vector<std::pair<std::string, int>> vec(freq.begin(), freq.end());
    std::sort(vec.begin(), vec.end(),
              [](const auto& a, const auto& b) {
                  return a.second > b.second; // 빈도 높은 순
              });

    // 결과 문자열 만들기
    std::string out;
    out += "=== 텍스트 길이: " + std::to_string(text.size()) + " bytes ===\n";
    out += "=== 상위 10개 단어 ===\n";

    int count = 0;
    for (const auto& p : vec) {
        out += p.first + " : " + std::to_string(p.second) + "\n";
        if (++count >= 10) break;
    }

    out += "============================\n";

    // JS 쪽에서 읽을 수 있도록 힙에 복사해서 반환
    // (cwrap("string")이 이 메모리는 알아서 free 해줌)
    return strdup(out.c_str());
}

} // extern "C"
