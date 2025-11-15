// WASM 모듈 초기화 준비
let processText = null;


// UI 요소들
const fileInput = document.getElementById("fileInput");
const analyzeBtn = document.getElementById("analyzeBtn");
const logArea = document.getElementById("log");
const resultDiv = document.getElementById("result");

// 파일 이름이 표시될 span
const fileNameSpan = document.getElementById("fileName");

// 파일 선택 시 파일 이름 표시
fileInput.addEventListener("change", () => {
    if (fileInput.files && fileInput.files[0]) {
        fileNameSpan.textContent = fileInput.files[0].name;
    } else {
        fileNameSpan.textContent = "선택된 파일 없음";
    }
});

// ===============================
//  WASM 로딩 완료 시 실행
// ===============================
Module.onRuntimeInitialized = function () {
    processText = Module.cwrap("process_text", "string", ["string"]);

    // 초기 안내 문구
    logArea.textContent =
        "WASM 로드 완료!\n텍스트 파일을 선택한 뒤 '분석하기'를 눌러주세요.\n";
};

// ===============================
//  "분석하기" 버튼 클릭 이벤트
// ===============================
analyzeBtn.addEventListener("click", () => {
    const file = fileInput.files[0];

    if (!file) {
        logArea.textContent = "먼저 텍스트(.txt) 파일을 선택해 주세요.\n";
        return;
    }

    if (!processText) {
        logArea.textContent = "아직 WASM이 준비되지 않았습니다.\n";
        return;
    }

    const reader = new FileReader();

    reader.onload = function (e) {
        const text = e.target.result;

        // ⭐ 분석 시작 → 안내문구 삭제
        logArea.textContent = "";

        logArea.textContent += `파일을 읽었습니다. 길이: ${text.length} bytes\n`;

        // C++ WASM 함수 호출
        const result = processText(text);

        // 결과 출력
        resultDiv.textContent = result;

        logArea.textContent += "분석 완료! 결과가 화면에 표시되었습니다.\n";
    };

    reader.onerror = function () {
        logArea.textContent = "파일 읽기 중 오류가 발생했습니다.\n";
    };

    reader.readAsText(file, "utf-8");
});
