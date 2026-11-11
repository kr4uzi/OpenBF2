// [CHOSEN, not recovered] Owner-run Metal library gate; no rendering or engine integration.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <fstream>
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <vector>

namespace fs = std::filesystem;
static std::string csv(NSString *s)
{
    std::string value = s ? s.UTF8String : "", escaped = "\"";
    for (char c : value) { if (c == '"') escaped += '"'; escaped += c; }
    return escaped + '"';
}
static id jsonFile(const fs::path &path)
{
    NSData *data = [NSData dataWithContentsOfFile:@(path.c_str())];
    return data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
}
static int run(int argc, char **argv)
{
    if (argc != 3) throw std::runtime_error("usage: metal-compile MSL_DIRECTORY RESULTS.csv");
    fs::path directory = fs::canonical(argv[1]), results = argv[2];
    // Append to an existing sweep, never accidentally create a report with a different schema.
    std::ifstream check(results);
    std::string header;
    std::getline(check, header);
    if (header != "stage,file,defines,compile,parse,translate,error_summary,job,shader_count,msl_count,metal_file,metal")
        throw std::runtime_error("Missing or incompatible results.csv header");
    std::ofstream out(results, std::ios::app);
    if (!out) throw std::runtime_error("Cannot append results.csv");
    NSMutableDictionary *jobs = [NSMutableDictionary dictionary];
    id manifest = jsonFile(results.parent_path() / "jobs.json");
    if ([manifest isKindOfClass:[NSArray class]])
        for (NSDictionary *job in manifest) jobs[job[@"id"]] = job;
    std::vector<fs::path> paths;
    for (const auto &item : fs::recursive_directory_iterator(directory))
        if (item.is_regular_file() && item.path().extension() == ".metal") paths.push_back(item.path());
    std::sort(paths.begin(), paths.end());
    if (paths.empty()) { std::cerr << "No .metal files; no Metal gate exercised\n"; return 1; }
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) std::cerr << "MTLCreateSystemDefaultDevice returned nil; Metal compilation unavailable\n";
    else std::cout << "Metal device: " << device.name.UTF8String << '\n';
    unsigned passed = 0;
    for (const auto &path : paths) {
        @autoreleasepool {
            NSString *jobID = @(path.parent_path().filename().c_str());
            NSDictionary *job = jobs[jobID];
            NSString *file = job ? job[@"file"] : @(path.c_str());
            NSData *definesData = job ? [NSJSONSerialization dataWithJSONObject:job[@"defines"]
                options:NSJSONWritingSortedKeys error:nil] : nil;
            NSString *defines = definesData ? [[NSString alloc] initWithData:definesData encoding:NSUTF8StringEncoding] : @"{}";
            NSDictionary *parsed = jsonFile(path.parent_path() / "result.json");
            NSString *entry = nil;
            for (NSDictionary *shader in parsed[@"shaders"])
                if ([shader[@"metal_file"] isEqual:@(path.filename().c_str())]) entry = shader[@"entry"];
            NSString *status = @"unavailable", *message = @"MTLCreateSystemDefaultDevice returned nil";
            if (device) {
                NSError *error = nil;
                NSString *source = [NSString stringWithContentsOfFile:@(path.c_str()) encoding:NSUTF8StringEncoding error:&error];
                id<MTLLibrary> library = source ? [device newLibraryWithSource:source options:nil error:&error] : nil;
                bool ok = library && (!entry || [library newFunctionWithName:entry]);
                status = ok ? @"ok" : @"fail";
                message = error.localizedDescription ?: (ok ? @"" : @"Metal entry missing");
                if (ok) ++passed;
            }
            NSError *writeError = nil;
            if (![message writeToFile:@((path.string() + ".log").c_str()) atomically:YES
                encoding:NSUTF8StringEncoding error:&writeError])
                throw std::runtime_error("Cannot write Metal diagnostic");
            out << "metal," << csv(file) << ',' << csv(defines)
                << ",not_run,not_run,not_run," << csv(message) << ',' << csv(jobID)
                << ",0,0," << csv(@(fs::relative(path, directory).c_str())) << ',' << csv(status) << '\n';
            out.flush();
            if (!out) throw std::runtime_error("Cannot append Metal result");
        }
    }
    std::cout << "Metal libraries: " << passed << '/' << paths.size() << " (unavailable=" << (!device) << ")\n";
    return !device ? 3 : passed == paths.size() ? 0 : 1;
}
int main(int argc, char **argv)
{
    @autoreleasepool {
        try { return run(argc, argv); }
        catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 2; }
    }
}
