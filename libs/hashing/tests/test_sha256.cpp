#include "hashing/sha256.hpp" // ELI5: test the public hashing functions.

#include <chrono> // ELI5: make a unique temporary test filename.
#include <filesystem> // ELI5: put and remove the temporary file.
#include <fstream> // ELI5: write bytes into the test file.
#include <iostream> // ELI5: print test results.
#include <stdexcept> // ELI5: stop when an expected result is wrong.
#include <string> // ELI5: hold test content and error text.
#include <string_view> // ELI5: pass text to the hashing helpers.

namespace { // ELI5: keep small test helpers private to this file.

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message); // ELI5: fail the test with a readable message if the condition is false.
}

struct TemporaryFile { // ELI5: manage a scratch file that will be deleted after the test.
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("cn-hashing-test-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    ~TemporaryFile() { // ELI5: clean up automatically when this helper goes away.
        std::error_code ignored; // ELI5: keep cleanup errors from interrupting the test result.
        std::filesystem::remove(path, ignored); // ELI5: delete the temporary input file.
    }
};

} // ELI5: finish the private test helper area.

int main() { // ELI5: run known-answer and file-based SHA-256 checks.
    try {
        // ELI5: compare SHA-256 against published expected answers for empty input and "abc".
        require(cn::hashing::to_hex(cn::hashing::sha256(std::string_view{})) ==
                    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                "empty SHA-256 vector did not match");
        require(cn::hashing::to_hex(cn::hashing::sha256("abc")) ==
                    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "abc SHA-256 vector did not match");

        // ELI5: make sure hashing the same text in several pieces matches hashing it all at once.
        cn::hashing::Sha256 incremental;
        incremental.update(std::string_view("a"));
        incremental.update(std::string_view("b"));
        incremental.update(std::string_view("c"));
        const auto expected = cn::hashing::sha256("abc");
        require(incremental.finish() == expected, "streaming updates differed from one-shot hashing");

        // ELI5: check that hex conversion accepts uppercase input and prints lowercase output.
        const auto uppercase = cn::hashing::from_hex(
            "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
        require(uppercase == expected, "hexadecimal decoding did not accept uppercase digits");
        require(cn::hashing::to_hex(uppercase) ==
                    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                "hexadecimal encoding was not lowercase");

        // ELI5: make sure malformed hex is rejected for the wrong length and for a bad character.
        bool bad_length = false;
        try { (void)cn::hashing::from_hex("abc"); }
        catch (const std::invalid_argument&) { bad_length = true; }
        require(bad_length, "hex decoder accepted the wrong digest length");

        bool bad_digit = false;
        try { (void)cn::hashing::from_hex(std::string(63, '0') + "z"); }
        catch (const std::invalid_argument&) { bad_digit = true; }
        require(bad_digit, "hex decoder accepted a non-hexadecimal digit");

        // ELI5: write text containing a zero byte, then check file hashing preserves every byte.
        TemporaryFile file;
        {
            std::ofstream output(file.path, std::ios::binary);
            const char content[] = {'a', '\0', 'b', '\n'};
            output.write(content, sizeof(content));
            require(static_cast<bool>(output), "could not write temporary hash input");
        }
        std::uint64_t size = 0; // ELI5: ask the file helper to tell us the input's byte count.
        const auto file_hash = cn::hashing::sha256_file(file.path.string(), &size);
        const cn::Bytes file_bytes{'a', 0, 'b', '\n'};
        require(file_hash == cn::hashing::sha256(file_bytes), "file hash differed from byte hashing");
        require(size == file_bytes.size(), "file hash reported an incorrect byte count");

        std::cout << "hashing behavioral checks passed\n"; // ELI5: all expected hash answers matched.
        return 0; // ELI5: report success to the test runner.
    } catch (const std::exception& error) {
        std::cerr << "hashing test failed: " << error.what() << '\n'; // ELI5: show which check failed.
        return 1; // ELI5: report failure to the test runner.
    }
}
