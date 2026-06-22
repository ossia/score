; ModuleID = 'boom.cpp'
source_filename = "boom.cpp"
target datalayout = "e-m:w-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-w64-windows-gnu"

$_Z4boomi = comdat any

@keep = dso_local global ptr @_Z4boomi, align 8
@_ZTIi = external constant ptr

; Function Attrs: mustprogress noinline optnone uwtable
define linkonce_odr dso_local void @_Z4boomi(i32 noundef %0) #0 comdat {
  %2 = alloca i32, align 4
  store i32 %0, ptr %2, align 4
  %3 = load i32, ptr %2, align 4
  %4 = icmp ne i32 %3, 0
  br i1 %4, label %5, label %8

5:                                                ; preds = %1
  %6 = call ptr @__cxa_allocate_exception(i64 4) #1
  %7 = load i32, ptr %2, align 4
  store i32 %7, ptr %6, align 16
  call void @__cxa_throw(ptr %6, ptr @_ZTIi, ptr null) #2
  unreachable

8:                                                ; preds = %1
  ret void
}

declare dso_local ptr @__cxa_allocate_exception(i64)

declare dso_local void @__cxa_throw(ptr, ptr, ptr)

attributes #0 = { mustprogress noinline optnone uwtable "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nounwind }
attributes #2 = { noreturn }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!2, !3, !4, !5, !6}
!llvm.ident = !{!7}

!0 = distinct !DICompileUnit(language: DW_LANG_C_plus_plus_14, file: !1, producer: "clang version 21.1.3 (https://github.com/llvm/llvm-project 450f52eec88f728c89a9efd667dbeaf2dad93826)", isOptimized: false, runtimeVersion: 0, emissionKind: NoDebug, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "boom.cpp", directory: "/tmp/mre")
!2 = !{i32 2, !"Debug Info Version", i32 3}
!3 = !{i32 1, !"wchar_size", i32 2}
!4 = !{i32 8, !"PIC Level", i32 2}
!5 = !{i32 7, !"uwtable", i32 2}
!6 = !{i32 1, !"MaxTLSAlign", i32 65536}
!7 = !{!"clang version 21.1.3 (https://github.com/llvm/llvm-project 450f52eec88f728c89a9efd667dbeaf2dad93826)"}
